/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "screen_grid.h"

#include "clar_asserts.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "font_resource_keys.auto.h"
#include "pbl/util/math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_GRID_PADDING 5
#define SCREEN_GRID_ONE_BIT PBL_IF_COLOR_ELSE(false, true)
#define SCREEN_GRID_BACKGROUND PBL_IF_COLOR_ELSE(GColorShockingPinkARGB8, 0)

//! Defined by fw/graphics/util.h, which the test includes
bool gbitmap_pbi_eq(GBitmap *bmp, const char *filename);

static const char *const s_screen_grid_size_names[NumPreferredContentSizes] = {
  [PreferredContentSizeSmall] = "Small",
  [PreferredContentSizeMedium] = "Medium",
  [PreferredContentSizeLarge] = "Large",
  [PreferredContentSizeExtraLarge] = "Extra Large",
};

void screen_grid_init(ScreenGrid *grid, unsigned int num_rows) {
  cl_assert(num_rows <= SCREEN_GRID_MAX_ROWS);
  *grid = (ScreenGrid){
    .num_sizes = MIN(PreferredContentSizeDefault + 2, NumPreferredContentSizes),
    .num_rows = num_rows,
  };
}

static uint8_t *prv_screen_grid_read(const GBitmap *screen) {
  uint8_t *pixels = malloc(DISP_COLS * DISP_ROWS);
  memset(pixels, SCREEN_GRID_BACKGROUND, DISP_COLS * DISP_ROWS);
  for (int16_t y = 0; y < DISP_ROWS; y++) {
    const GBitmapDataRowInfo row = gbitmap_get_data_row_info(screen, y);
    for (int16_t x = row.min_x; x <= row.max_x; x++) {
      pixels[y * DISP_COLS + x] =
          SCREEN_GRID_ONE_BIT ? ((row.data[x / 8] >> (x % 8)) & 1) : row.data[x];
    }
  }
  return pixels;
}

//! Labels the screen in ctx with the size it repeats
static void prv_screen_grid_draw_same_as(GContext *ctx, PreferredContentSize size) {
  const GDrawState saved = graphics_context_get_drawing_state(ctx);
  graphics_context_set_default_drawing_state(ctx, GContextInitializationMode_System);
  const GRect label = GRect(10, DISP_ROWS / 2 - 28, DISP_COLS - 20, 60);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &label);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &label);
  char text[32];
  snprintf(text, sizeof(text), "Same as\n%s", s_screen_grid_size_names[size]);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, DISP_ROWS / 2 - 30, DISP_COLS, 60), GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_drawing_state(ctx, saved);
}

void screen_grid_add(ScreenGrid *grid, GContext *ctx, PreferredContentSize size,
                     unsigned int row) {
  cl_assert(size < grid->num_sizes && row < grid->num_rows && !grid->rendered[row][size]);
  grid->rendered[row][size] = prv_screen_grid_read(&ctx->dest_bitmap);
  for (PreferredContentSize earlier = 0; earlier < size; earlier++) {
    if (!memcmp(grid->rendered[row][earlier], grid->rendered[row][size], DISP_COLS * DISP_ROWS)) {
      prv_screen_grid_draw_same_as(ctx, earlier);
      grid->shown[row][size] = prv_screen_grid_read(&ctx->dest_bitmap);
      return;
    }
  }
}

//! True if every screen matches the Small screen in its row
static bool prv_screen_grid_is_small_only(const ScreenGrid *grid) {
  for (unsigned int row = 0; row < grid->num_rows; row++) {
    for (PreferredContentSize size = 1; size < grid->num_sizes; size++) {
      if (!grid->shown[row][size]) {
        return false;
      }
    }
  }
  return true;
}

static GBitmap *prv_screen_grid_create_bitmap(const ScreenGrid *grid) {
  const unsigned int num_columns = prv_screen_grid_is_small_only(grid) ? 1 : grid->num_sizes;
  // Padding separates columns and rows only when there are several
  const int16_t pad_x = (num_columns > 1) ? SCREEN_GRID_PADDING : 0;
  const int16_t pad_y = (grid->num_rows > 1) ? SCREEN_GRID_PADDING : 0;
  const GSize bitmap_size = GSize(pad_x + num_columns * (DISP_COLS + pad_x),
                                  pad_y + grid->num_rows * (DISP_ROWS + pad_y));
  GBitmap *bitmap = gbitmap_create_blank(
      bitmap_size, SCREEN_GRID_ONE_BIT ? GBitmapFormat1Bit : GBitmapFormat8Bit);
  memset(bitmap->addr, SCREEN_GRID_BACKGROUND, bitmap->row_size_bytes * bitmap_size.h);

  for (unsigned int row = 0; row < grid->num_rows; row++) {
    for (unsigned int column = 0; column < num_columns; column++) {
      const uint8_t *pixels = grid->shown[row][column] ?: grid->rendered[row][column];
      cl_assert(pixels);
      const int16_t x_offset = pad_x + column * (DISP_COLS + pad_x);
      uint8_t *dest = (uint8_t *)bitmap->addr + (pad_y + row * (DISP_ROWS + pad_y)) *
                                                   bitmap->row_size_bytes;
      for (int16_t y = 0; y < DISP_ROWS; y++, dest += bitmap->row_size_bytes) {
        for (int16_t x = 0; x < DISP_COLS; x++) {
          const uint8_t pixel = pixels[y * DISP_COLS + x];
          const int16_t dest_x = x_offset + x;
          if (!SCREEN_GRID_ONE_BIT) {
            dest[dest_x] = pixel;
          } else if (pixel) {
            dest[dest_x / 8] |= 1 << (dest_x % 8);
          }
        }
      }
    }
  }
  return bitmap;
}

void screen_grid_check(ScreenGrid *grid, const char *pbi_file) {
  GBitmap *bitmap = prv_screen_grid_create_bitmap(grid);
  cl_check(gbitmap_pbi_eq(bitmap, pbi_file));
  gbitmap_destroy(bitmap);
  for (unsigned int row = 0; row < SCREEN_GRID_MAX_ROWS; row++) {
    for (PreferredContentSize size = 0; size < NumPreferredContentSizes; size++) {
      free(grid->rendered[row][size]);
      free(grid->shown[row][size]);
    }
  }
}

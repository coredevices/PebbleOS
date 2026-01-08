/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "compositor.h"

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gtypes.h"
#include "util/bitset.h"
#include "util/math.h"
#include "util/size.h"

#include <string.h>

//! This variable is used when we are flushing s_framebuffer out to the display driver.
//! It's set to the current row index that we are DMA'ing out to the display.
static uint16_t s_current_flush_line;

static void (*s_update_complete_handler)(void);

#if PLATFORM_SILK || PLATFORM_ASTERIX
static const uint8_t s_corner_shape[] = { 3, 1, 1 };
static uint8_t s_line_buffer[FRAMEBUFFER_BYTES_PER_ROW];
#endif

#if PLATFORM_OBELIX
static const uint8_t s_corner_shape[] = { 12, 9, 7, 6, 5, 4, 3, 2, 2, 1, 1, 1 };
// For Obelix, we modify the framebuffer directly because the display driver
// does in-place pixel format conversion and expects row.data to point into
// the compositor's framebuffer. We save original corner pixels here to restore later.
// Max corner pixels per row = 12, rows with corners = 12 top + 12 bottom = 24,
// 2 corners per row (left+right), so 12 * 24 * 2 = 576 bytes
#define CORNER_SAVE_ROWS ARRAY_LENGTH(s_corner_shape)
static uint8_t s_saved_corners[CORNER_SAVE_ROWS * 2][12 * 2]; // [row][left+right pixels]
static uint8_t s_dirty_y0;
static uint8_t s_dirty_y1;
#endif

//! display_update get next line callback
static bool prv_flush_get_next_line_cb(DisplayRow* row) {
  FrameBuffer *fb = compositor_get_framebuffer();

  s_current_flush_line = MAX(s_current_flush_line, fb->dirty_rect.origin.y);
  const uint16_t y_end = fb->dirty_rect.origin.y + fb->dirty_rect.size.h;
  if (s_current_flush_line < y_end) {
    row->address = s_current_flush_line;
    void *fb_line = framebuffer_get_line(fb, s_current_flush_line);
#if PLATFORM_SILK || PLATFORM_ASTERIX
    // Draw rounded corners onto the screen without modifying the
    // system framebuffer.
    if (s_current_flush_line < ARRAY_LENGTH(s_corner_shape) ||
        s_current_flush_line >= DISP_ROWS - ARRAY_LENGTH(s_corner_shape)) {
      memcpy(s_line_buffer, fb_line, FRAMEBUFFER_BYTES_PER_ROW);
      uint8_t corner_idx =
        (s_current_flush_line < ARRAY_LENGTH(s_corner_shape))?
        s_current_flush_line : DISP_ROWS - s_current_flush_line - 1;
      uint8_t corner_width = s_corner_shape[corner_idx];
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        bitset8_clear(s_line_buffer, pixel);
        bitset8_clear(s_line_buffer, DISP_COLS - pixel - 1);
      }
      row->data = s_line_buffer;
    } else {
      row->data = fb_line;
    }
#elif PLATFORM_OBELIX
    // Draw rounded corners by modifying the framebuffer directly.
    // The display driver does in-place format conversion and expects row.data
    // to point into the compositor's framebuffer. We save and restore corners.
    if (s_current_flush_line < ARRAY_LENGTH(s_corner_shape) ||
        s_current_flush_line >= DISP_ROWS - ARRAY_LENGTH(s_corner_shape)) {
      uint8_t corner_idx =
        (s_current_flush_line < ARRAY_LENGTH(s_corner_shape))?
        s_current_flush_line : DISP_ROWS - s_current_flush_line - 1;
      uint8_t save_idx =
        (s_current_flush_line < ARRAY_LENGTH(s_corner_shape))?
        s_current_flush_line : CORNER_SAVE_ROWS + corner_idx;
      uint8_t corner_width = s_corner_shape[corner_idx];
      uint8_t *line = fb_line;
      // Save original corner pixels
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        s_saved_corners[save_idx][pixel] = line[pixel];
        s_saved_corners[save_idx][12 + pixel] = line[DISP_COLS - pixel - 1];
      }
      // Draw black corners
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        line[pixel] = GColorBlackARGB8;
        line[DISP_COLS - pixel - 1] = GColorBlackARGB8;
      }
    }
    row->data = fb_line;
#else
    row->data = fb_line;
#endif
    s_current_flush_line++;
    return true;
  }

  return false;
}

//! display_update complete callback
static void prv_flush_complete_cb(void) {
#if PLATFORM_OBELIX
  // Restore original corner pixels that we modified before the display update
  FrameBuffer *fb = compositor_get_framebuffer();
  for (uint8_t i = 0; i < CORNER_SAVE_ROWS; ++i) {
    uint8_t corner_width = s_corner_shape[i];
    // Top corners (only if row was in dirty region)
    if (i >= s_dirty_y0 && i <= s_dirty_y1) {
      uint8_t *top_line = framebuffer_get_line(fb, i);
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        top_line[pixel] = s_saved_corners[i][pixel];
        top_line[DISP_COLS - pixel - 1] = s_saved_corners[i][12 + pixel];
      }
    }
    // Bottom corners (only if row was in dirty region)
    uint8_t bottom_row = DISP_ROWS - i - 1;
    if (bottom_row >= s_dirty_y0 && bottom_row <= s_dirty_y1) {
      uint8_t *bottom_line = framebuffer_get_line(fb, bottom_row);
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        bottom_line[pixel] = s_saved_corners[CORNER_SAVE_ROWS + i][pixel];
        bottom_line[DISP_COLS - pixel - 1] = s_saved_corners[CORNER_SAVE_ROWS + i][12 + pixel];
      }
    }
  }
#endif

  s_current_flush_line = 0;
  framebuffer_reset_dirty(compositor_get_framebuffer());

  if (s_update_complete_handler) {
    s_update_complete_handler();
  }
}

void compositor_display_update(void (*handle_update_complete_cb)(void)) {
  FrameBuffer *fb = compositor_get_framebuffer();
  if (!framebuffer_is_dirty(fb)) {
    return;
  }
#if PLATFORM_GETAFIX
  // Force full screen updates - partial ROI causes animation issues on getafix display
  fb->dirty_rect = (GRect){ GPointZero, fb->size };
#endif
#if PLATFORM_OBELIX
  // Capture dirty region bounds for corner restoration later
  s_dirty_y0 = fb->dirty_rect.origin.y;
  s_dirty_y1 = fb->dirty_rect.origin.y + fb->dirty_rect.size.h - 1;
#endif
  s_update_complete_handler = handle_update_complete_cb;
  s_current_flush_line = 0;

  display_update(&prv_flush_get_next_line_cb, &prv_flush_complete_cb);
}

bool compositor_display_update_in_progress(void) {
  return display_update_in_progress();
}

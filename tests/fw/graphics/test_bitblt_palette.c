/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/bitblt.h"
#include "applib/graphics/bitblt_private.h"
#include "applib/graphics/8_bit/framebuffer.h"


#include "clar.h"

#include <string.h>

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "test_graphics.h"

// Setup
////////////////////////////////////
static GContext ctx;
static FrameBuffer framebuffer;
static uint8_t dest_bitmap_data[FRAMEBUFFER_SIZE_BYTES];
static GBitmap dest_bitmap = {
  .addr = dest_bitmap_data,
  .row_size_bytes = FRAMEBUFFER_BYTES_PER_ROW,
  .info.is_bitmap_heap_allocated = false,
  .info.format = GBitmapFormat8Bit,
  .info.version = GBITMAP_VERSION_CURRENT,
  .bounds = {
    .size = {
      .w = DISP_COLS,
      .h = DISP_ROWS
    },
    .origin = { 0, 0 },
  },
};

// Utilities
////////////////////////////////////
#if SCREEN_COLOR_DEPTH_BITS == 8
extern GColor get_bitmap_color(GBitmap *bmp, int x, int y);
#endif

// @param color_index the color index (the value) to put into the bitmap at (x, y).
// @param bpp Bits per pixel.
// @para line_stride how many bytes per line in the bitmap data.
void packed_pixel_set(uint8_t *buf, uint8_t color_index, int16_t x, int16_t y,
                      uint8_t bpp, int16_t line_stride) {
  const uint8_t ppb = 8 / bpp;
  uint8_t idx = y*line_stride + (x/(ppb));
  const uint8_t shift = (8 - bpp) - bpp * (x % ppb);
  const uint8_t mask = ~(((1 << bpp) - 1) << shift);
  buf[idx] = buf[idx] & mask;
  buf[idx] |= ((color_index & ((1 << bpp) - 1)) << shift);
}

static bool prv_check_source_stripe_blit(const uint8_t *data,
                                         const GBitmap *src_bmp, GColor surround_color) {
  for (uint8_t y = 0; y < (FRAMEBUFFER_SIZE_BYTES / FRAMEBUFFER_BYTES_PER_ROW); ++y) {
    for (uint8_t x = 0; x < FRAMEBUFFER_BYTES_PER_ROW; ++x) {
      uint8_t color = data[y*FRAMEBUFFER_BYTES_PER_ROW + x];
      if (y < src_bmp->bounds.size.h && x < src_bmp->bounds.size.w) {
        if (color != src_bmp->palette[x].argb) {
          return false;
        }
      } else {
        if (color != surround_color.argb) {
          return false;
        }
      }
    }
  }
  return true;
}

// Tests
////////////////////////////////////

// setup and teardown
void test_bitblt_palette__initialize(void) {
  framebuffer_init(&framebuffer, &(GSize) { DISP_COLS, DISP_ROWS });
  test_graphics_context_init(&ctx, &framebuffer);
}

void test_bitblt_palette__cleanup(void) {
}

void test_bitblt_palette__1Bit_color(void) {
  const int BITS_PER_PIXEL = 1;
  const int PIXELS_PER_BYTE = (8 / BITS_PER_PIXEL);
  const int WIDTH = 2;
  const int HEIGHT = 2;
  const int ROW_STRIDE = (WIDTH + (PIXELS_PER_BYTE - 1)) / PIXELS_PER_BYTE;

  uint8_t s_data[ROW_STRIDE * HEIGHT];
  GColor s_palette[1 << 1] = {
    GColorMelon, GColorIcterine
  };
  cl_assert(sizeof(s_palette) == (1 << BITS_PER_PIXEL));
  GBitmap s_bmp = (GBitmap) {
    .addr = s_data,
    .row_size_bytes = ROW_STRIDE,
    .info.format = GBitmapFormat1BitPalette,
    .info.version = GBITMAP_VERSION_CURRENT,
    .bounds = { .size = { WIDTH, HEIGHT } },
    .palette = s_palette,
  };
  memset(s_data, 0, sizeof(s_data));

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      packed_pixel_set(s_data, x /* color_index */, x, y, BITS_PER_PIXEL, s_bmp.row_size_bytes);
    }
  }
  memset(dest_bitmap_data, GColorWhite.argb, sizeof(dest_bitmap_data));

#if SCREEN_COLOR_DEPTH_BITS == 8
  char print_buf[20];
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      snprintf(print_buf, sizeof(print_buf), "Failed index = %d, %d", x, y);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
    }
  }
#endif
}

void test_bitblt_palette__4Bit_assign(void) {
  const int BITS_PER_PIXEL = 4;
  const int PIXELS_PER_BYTE = (8 / BITS_PER_PIXEL);
  const int WIDTH = 16;
  const int HEIGHT = 16;
  const int ROW_STRIDE = (WIDTH + (PIXELS_PER_BYTE - 1)) / PIXELS_PER_BYTE;

  uint8_t s_data[ROW_STRIDE * HEIGHT];
  GColor s_palette[1 << 4] = {
    GColorMelon,         GColorIcterine,   GColorYellow, GColorSunsetOrange,
    GColorScreaminGreen, GColorMagenta,    GColorOrange, GColorFolly,
    GColorLimerick,      GColorPictonBlue, GColorPurple, GColorCadetBlue,
    GColorMalachite,     GColorGreen,      GColorIndigo, GColorVividCerulean
  };
  cl_assert(sizeof(s_palette) == (1 << BITS_PER_PIXEL));
  GBitmap s_bmp = (GBitmap) {
    .addr = s_data,
    .row_size_bytes = ROW_STRIDE,
    .info.format = GBitmapFormat4BitPalette,
    .info.version = GBITMAP_VERSION_CURRENT,
    .bounds = { .size = { WIDTH, HEIGHT } },
    .palette = s_palette,
  };
  memset(s_data, 0, sizeof(s_data));

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      packed_pixel_set(s_data, x /* color_index */, x, y, BITS_PER_PIXEL, s_bmp.row_size_bytes);
    }
  }
  memset(dest_bitmap_data, GColorWhite.argb, sizeof(dest_bitmap_data));

#if SCREEN_COLOR_DEPTH_BITS == 8
  char print_buf[20];
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      snprintf(print_buf, sizeof(print_buf), "Failed index = %d, %d", x, y);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
    }
  }
#endif

  bitblt_bitmap_into_bitmap(&dest_bitmap, &s_bmp, GPointZero, GCompOpAssign, GColorWhite);

  cl_assert(prv_check_source_stripe_blit(dest_bitmap_data, &s_bmp, GColorWhite));
}

static void prv_opaque_2bit_simple(GCompOp compositing_mode) {
  const int BITS_PER_PIXEL = 2;
  const int PIXELS_PER_BYTE = (8 / BITS_PER_PIXEL);
  const int WIDTH = 4;
  const int HEIGHT = 4;
  const int ROW_STRIDE = (WIDTH + (PIXELS_PER_BYTE - 1)) / PIXELS_PER_BYTE;

  uint8_t s_data[ROW_STRIDE * HEIGHT];
  GColor s_palette[] = {
    GColorRed, GColorWhite, GColorBlack, GColorBlue
  };

  cl_assert(sizeof(s_palette) == (1 << BITS_PER_PIXEL));

  GBitmap s_bmp = (GBitmap) {
    .addr = s_data,
    .row_size_bytes = ROW_STRIDE,
    .info.format = GBitmapFormat2BitPalette,
    .info.version = GBITMAP_VERSION_CURRENT,
    .bounds = { .size = { WIDTH, HEIGHT } },
    .palette = s_palette,
  };

  memset(s_data, 0, sizeof(s_data));

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      packed_pixel_set(s_data, x /* color_index */, x, y, BITS_PER_PIXEL, s_bmp.row_size_bytes);
    }
  }

  memset(dest_bitmap_data, GColorWhite.argb, sizeof(dest_bitmap_data));

#if SCREEN_COLOR_DEPTH_BITS == 8
  char print_buf[20];
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      snprintf(print_buf, sizeof(print_buf), "Failed index = %d, %d", x, y);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
      cl_check_(gcolor_equal(get_bitmap_color(&s_bmp, x, y), s_palette[x]), print_buf);
    }
  }
#endif

  bitblt_bitmap_into_bitmap(&dest_bitmap, &s_bmp, GPointZero, compositing_mode, GColorWhite);

  switch (compositing_mode) {
    case GCompOpTint: {
      // Since this is opaque, the tint_color will be used for the RGB value of the destination
      // image, so we want to use that as our palette.
      for (uint8_t idx = 0; idx < WIDTH; idx++) {
        if (s_palette[idx].a == 3) {
          s_palette[idx] = GColorWhite;
        }
      }
      break;
    }
    default:
      break;
  }

  cl_assert(prv_check_source_stripe_blit(dest_bitmap_data, &s_bmp, GColorWhite));
}

static void prv_4bit_simple(GCompOp compositing_mode, GColor color, bool transparent) {
  const int BITS_PER_PIXEL = 4;
  const int PIXELS_PER_BYTE = (8 / BITS_PER_PIXEL);
  const int WIDTH = 16;
  const int HEIGHT = 16;
  const int ROW_STRIDE = (WIDTH + (PIXELS_PER_BYTE - 1)) / PIXELS_PER_BYTE;

  uint8_t s_data[ROW_STRIDE * HEIGHT];
  GColor s_palette[] = {
    GColorMelon,         GColorIcterine,   GColorYellow, GColorSunsetOrange,
    GColorScreaminGreen, GColorMagenta,    GColorOrange, GColorFolly,
    GColorLimerick,      GColorPictonBlue, GColorPurple, GColorCadetBlue,
    GColorMalachite,     GColorGreen,      GColorIndigo, GColorVividCerulean
  };

  cl_assert(sizeof(s_palette) == (1 << BITS_PER_PIXEL));

  if (transparent) {
    // Make all of those colors fully transparent
    for (int i = 0; i < sizeof(s_palette); ++i) {
      s_palette[i].a = 0;
    }
  }

  GBitmap s_bmp = (GBitmap) {
    .addr = s_data,
    .row_size_bytes = ROW_STRIDE,
    .info.format = GBitmapFormat4BitPalette,
    .info.version = GBITMAP_VERSION_CURRENT,
    .bounds = { .size = { WIDTH, HEIGHT } },
    .palette = s_palette,
  };

  memset(s_data, 0, sizeof(s_data));

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      packed_pixel_set(s_data, x /* color_index */, x, y, BITS_PER_PIXEL, s_bmp.row_size_bytes);
    }
  }

  memset(dest_bitmap_data, GColorWhite.argb, sizeof(dest_bitmap_data));

  bitblt_bitmap_into_bitmap(&dest_bitmap, &s_bmp, GPointZero, compositing_mode, color);

  switch (compositing_mode) {
    case GCompOpSet: {
      if (transparent) {
        memset(s_palette, color.argb, sizeof(s_palette));
      }
      break;
    }
    case GCompOpAssign:
      break;
    case GCompOpTint: {
      for (uint8_t idx = 0; idx < WIDTH; idx++) {
        if (s_palette[idx].a == 3 || transparent) {
          s_palette[idx] = color;
        }
      }
      break;
    }
    default:
      break;
  }

  cl_assert(prv_check_source_stripe_blit(dest_bitmap_data, &s_bmp, GColorWhite));
}

void test_bitblt_palette__2Bit_assign_opaque(void) {
  prv_opaque_2bit_simple(GCompOpAssign);
}

void test_bitblt_palette__2Bit_set_opaque(void) {
  prv_opaque_2bit_simple(GCompOpSet);
}

void test_bitblt_palette__2Bit_comptint_opaque(void) {
  prv_opaque_2bit_simple(GCompOpTint);
}

void test_bitblt_palette__4Bit_assign_opaque(void) {
  prv_4bit_simple(GCompOpAssign, GColorWhite, false /* opaque */);
}

void test_bitblt_palette__4Bit_assign_transparent(void) {
  prv_4bit_simple(GCompOpAssign, GColorWhite, true /* transparent */);
}

void test_bitblt_palette__4Bit_set_opaque(void) {
  prv_4bit_simple(GCompOpSet, GColorWhite, false /* opaque */);
}

void test_bitblt_palette__4Bit_set_transparent(void) {
  prv_4bit_simple(GCompOpSet, GColorWhite, true /* transparent */);
}

void test_bitblt_palette__4Bit_comptint_opaque(void) {
  prv_4bit_simple(GCompOpTint, GColorBlack, false /* opaque */);
  prv_4bit_simple(GCompOpTint, GColorBlue, false /* opaque */);
}

void test_bitblt_palette__4Bit_comptint_transparent(void) {
  prv_4bit_simple(GCompOpTint, GColorWhite, true /* transparent */);
}

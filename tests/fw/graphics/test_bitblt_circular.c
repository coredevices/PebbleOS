/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/gtypes.h"
#include "applib/graphics/bitblt.h"

#include "clar.h"
#include "util.h"

#include <string.h>
#include <stdio.h>

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"

#if SCREEN_COLOR_DEPTH_BITS == 8
  #include "8bit/test_framebuffer.h"
#else
  Static_assert(false, "These tests are only for color displays");
#endif

static GContext *ctx;
static FrameBuffer *fb = NULL;

// Setup
void test_bitblt_circular__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
  ctx = malloc(sizeof(GContext));
  test_graphics_context_init(ctx, fb);
  framebuffer_clear(fb);
}

// Teardown
void test_bitblt_circular__cleanup(void) {
  free(fb);
  free(ctx);
}

// Tests
////////////////////////////////////

// Reference PNGs reside in "tests/test_images/"
// and are created at build time, with the test PBI file generated 
// by bitmapgen.py from the reference PNG copied to TEST_IMAGES_PATH
// covers 1-bit b&w image
// covers 1,2,4,8 bit palettized

// Tests 1-bit black&white PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_1_bit_bw(void) {
  const char test_file[] = "test_bitblt_circular__color_1_bit_bw.1bit.pbi";
  GBitmap *bitmap = setup_pbi_test(test_file);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  native_framebuffer->info.format = GBitmapFormat8BitCircular;
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, test_file));
}

// Tests 1-bit red&white palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_1_bit(void) {
  GBitmap *bitmap = setup_png_test(TEST_PNG_FILE);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

// Tests 2-bit palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_2_bit(void) {
  GBitmap *bitmap = setup_png_test(TEST_PNG_FILE);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

// Tests 4-bit bitblt palettized to circular display buffer
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_4_bit(void) {
  GBitmap *bitmap = setup_png_test(TEST_PNG_FILE);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

// Tests 8-bit bitblt to circular display buffer
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_8_bit(void) {
  GBitmap *bitmap = setup_png_test(TEST_PNG_FILE);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

// Tests 8-bit bitblt tiling support to circular display buffer
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_8_bit_tiling(void) {
  GBitmap *bitmap = setup_png_test(TEST_NAMED_PNG_FILE("test_bitblt_circular__tile"));
  cl_assert(bitmap->info.format == GBitmapFormat8Bit);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap_tiled(
      native_framebuffer, bitmap, native_framebuffer->bounds, 
      GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

// Tests palettized bitblt non-power-of-two tiling support to circular display buffer
// Result:
//   - gbitmap matches platform loaded PBI
void test_bitblt_circular__color_8_bit_tiling_palettized(void) {
  GBitmap *bitmap = setup_png_test(TEST_NAMED_PNG_FILE("test_bitblt_circular__tile_palettized"));
  cl_assert(bitmap->info.format == GBitmapFormat2BitPalette);
  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);
  bitblt_bitmap_into_bitmap_tiled(
      native_framebuffer, bitmap, native_framebuffer->bounds, 
      GPointZero, GCompOpAssign, GColorWhite);
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

void test_bitblt_circular__8_bit_converted_circular(void) {
  GBitmap *bitmap = setup_png_test(TEST_NAMED_PNG_FILE("test_bitblt_circular__spiral"));
  cl_assert(bitmap->info.format == GBitmapFormat8Bit);

  // Convert input PNG from rectangular to circular
  gbitmap_8bit_to_8bit_circular(bitmap);
  cl_assert(bitmap->info.format == GBitmapFormat8BitCircular);

  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  GRect framebuffer_bounds = gbitmap_get_bounds(native_framebuffer);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);

  // Set screen to black to match empty region with test image
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &framebuffer_bounds);

  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPointZero, GCompOpAssign, GColorWhite); 
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

void test_bitblt_circular__8_bit_converted_circular_offset(void) {
  GBitmap *bitmap = setup_png_test(TEST_NAMED_PNG_FILE("test_bitblt_circular__spiral"));
  cl_assert(bitmap->info.format == GBitmapFormat8Bit);

  // Convert input PNG from rectangular to circular
  gbitmap_8bit_to_8bit_circular(bitmap);
  cl_assert(bitmap->info.format == GBitmapFormat8BitCircular);

  GBitmap *native_framebuffer = graphics_context_get_bitmap(ctx);
  GRect framebuffer_bounds = gbitmap_get_bounds(native_framebuffer);
  cl_assert(native_framebuffer->info.format == GBitmapFormat8BitCircular);
  cl_assert(DISPLAY_FRAMEBUFFER_BYTES == 25944);

  // Set screen to black to match empty region with test image
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &framebuffer_bounds);

  // Shift the output by non-power-of-2 values for testing
  bitblt_bitmap_into_bitmap(native_framebuffer, bitmap, GPoint(33, 10), GCompOpAssign, GColorWhite); 
  cl_assert(gbitmap_pbi_eq(native_framebuffer, TEST_PBI_FILE));
}

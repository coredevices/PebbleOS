/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/light/als_screen_compensation.h"
#include "applib/graphics/gtypes.h"
#include <pbl/drivers/ambient_light.h>  // AMBIENT_LIGHT_LEVEL_MAX

#include <string.h>

// Q8 unity gain and black-scale clamps used throughout.
#define UNITY_Q8 (256u)
#define SCALE_8X_Q8 (8u * 256u)
#define SCALE_32X_Q8 (32u * 256u)

// 8-bit pixel bytes for fully-opaque white / black (argb: a=3).
#define PX_WHITE (0xFFu)  // & 0x3F == 0x3F
#define PX_BLACK (0xC0u)  // & 0x3F == 0x00

// --- Fakes for the region helper's only external dependencies ---------------
// Real lookup lives in gtypes.c; we only need the white/black entries here so
// the region-averaging logic is exercised without linking the graphics libs.
const GColor8Component g_color_luminance_lookup[64] = {
  [0x00] = 0,  // black -> luminance 0
  [0x3F] = 3,  // white -> luminance 3 (max)
};

// Mirrors the non-circular branch of the real gbitmap_get_data_row_info().
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y) {
  return (GBitmapDataRowInfo){
      .data = (uint8_t *)bitmap->addr + y * bitmap->row_size_bytes,
      .min_x = 0,
      .max_x = (int16_t)(grect_get_max_x(&bitmap->bounds) - 1),
  };
}

static GBitmap prv_make_bitmap(uint8_t *pixels, int16_t w, int16_t h) {
  GBitmap b = {0};
  b.addr = pixels;
  b.row_size_bytes = (uint16_t)w;
  b.info.format = GBitmapFormat8Bit;
  b.bounds = (GRect){.origin = {0, 0}, .size = {w, h}};
  return b;
}

void test_als_screen_compensation__initialize(void) {}
void test_als_screen_compensation__cleanup(void) {}

// --- als_compensation_apply() ----------------------------------------------

void test_als_screen_compensation__apply_white_is_unity(void) {
  // White (L=1) -> gain 1.0x regardless of black_scale.
  cl_assert_equal_i(als_compensation_apply(1000, UNITY_Q8, SCALE_8X_Q8), 1000);
}

void test_als_screen_compensation__apply_black_is_full_scale(void) {
  // Black (L=0) -> gain == black_scale.
  cl_assert_equal_i(als_compensation_apply(100, 0, SCALE_8X_Q8), 800);
}

void test_als_screen_compensation__apply_inverse_luminance(void) {
  // Inverse-luminance: gain(L) = 256/lum. Use a large black_scale so the dark
  // clamp doesn't engage. raw 256 -> corrected == gain (256/lum * 256 / 256).
  cl_assert_equal_i(als_compensation_apply(256, UNITY_Q8 / 2, SCALE_32X_Q8), 512);   // 2.00x
  cl_assert_equal_i(als_compensation_apply(256, 85, SCALE_32X_Q8), 771);             // ~3.01x
  cl_assert_equal_i(als_compensation_apply(256, 170, SCALE_32X_Q8), 385);            // ~1.51x
}

void test_als_screen_compensation__apply_clamps_dark_end(void) {
  // Near-black: 256/10 == 25.6x would exceed the 8x black_scale -> clamp to 8x.
  cl_assert_equal_i(als_compensation_apply(256, 10, SCALE_8X_Q8), 256 * 8);
}

void test_als_screen_compensation__apply_unity_scale_is_passthrough(void) {
  // black_scale == 1.0x disables the feature: corrected == raw for any L.
  cl_assert_equal_i(als_compensation_apply(1234, 0, UNITY_Q8), 1234);
  cl_assert_equal_i(als_compensation_apply(1234, UNITY_Q8, UNITY_Q8), 1234);
  cl_assert_equal_i(als_compensation_apply(1234, UNITY_Q8 / 2, UNITY_Q8), 1234);
}

void test_als_screen_compensation__apply_zero_raw(void) {
  cl_assert_equal_i(als_compensation_apply(0, 0, SCALE_8X_Q8), 0);
}

void test_als_screen_compensation__apply_clamps_to_max(void) {
  // 20000 * 8 == 160000 > AMBIENT_LIGHT_LEVEL_MAX (65536 for 16-bit).
  cl_assert_equal_i(als_compensation_apply(20000, 0, SCALE_8X_Q8), AMBIENT_LIGHT_LEVEL_MAX);
}

void test_als_screen_compensation__apply_no_overflow(void) {
  // Largest possible inputs must not overflow the 64-bit intermediate.
  cl_assert_equal_i(als_compensation_apply(65535, 0, 65535), AMBIENT_LIGHT_LEVEL_MAX);
}

void test_als_screen_compensation__apply_luminance_above_unity_clamped(void) {
  // avg_luminance_q8 > 256 is clamped to white (unity gain).
  cl_assert_equal_i(als_compensation_apply(1000, 1000, SCALE_8X_Q8), 1000);
}

// --- als_compensation_region_luminance() -----------------------------------

void test_als_screen_compensation__region_all_white(void) {
  uint8_t px[4 * 4];
  memset(px, PX_WHITE, sizeof(px));
  GBitmap b = prv_make_bitmap(px, 4, 4);
  cl_assert_equal_i(als_compensation_region_luminance(&b, 0, 0, 4, 4), 256);
}

void test_als_screen_compensation__region_all_black(void) {
  uint8_t px[4 * 4];
  memset(px, PX_BLACK, sizeof(px));
  GBitmap b = prv_make_bitmap(px, 4, 4);
  cl_assert_equal_i(als_compensation_region_luminance(&b, 0, 0, 4, 4), 0);
}

void test_als_screen_compensation__region_checkerboard(void) {
  // Half white, half black -> avg 255/0 == 127 (Q8).
  uint8_t px[2 * 2] = {PX_WHITE, PX_BLACK, PX_BLACK, PX_WHITE};
  GBitmap b = prv_make_bitmap(px, 2, 2);
  cl_assert_equal_i(als_compensation_region_luminance(&b, 0, 0, 2, 2), 127);
}

void test_als_screen_compensation__region_clamps_offscreen(void) {
  // A region extending past the bounds only counts the valid (white) pixels.
  uint8_t px[4 * 4];
  memset(px, PX_WHITE, sizeof(px));
  GBitmap b = prv_make_bitmap(px, 4, 4);
  cl_assert_equal_i(als_compensation_region_luminance(&b, 2, 2, 10, 10), 256);
}

void test_als_screen_compensation__region_empty_is_unity(void) {
  uint8_t px[4 * 4];
  memset(px, PX_BLACK, sizeof(px));
  GBitmap b = prv_make_bitmap(px, 4, 4);
  // Zero-area region and NULL bitmap both yield unity (256 == no-op gain).
  cl_assert_equal_i(als_compensation_region_luminance(&b, 0, 0, 0, 4), 256);
  cl_assert_equal_i(als_compensation_region_luminance(NULL, 0, 0, 4, 4), 256);
}

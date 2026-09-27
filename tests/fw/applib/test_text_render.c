/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/gcontext.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/text_render.h"
#include "applib/graphics/text_resources.h"

#include <string.h>

#include "clar.h"

#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_syscalls.h"

ResAppNum app_get_resource_num(void) {
  return 0;
}

size_t resource_size(ResAppNum app_num, uint32_t id) {
  return 0;
}

bool resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  return true;
}

static GBitmap *s_dest_bitmap;
static uint8_t s_font_data[64];
static FontResource s_font_res;

GBitmap *graphics_context_get_bitmap(GContext *ctx) {
  return s_dest_bitmap;
}

void graphics_context_mark_dirty_rect(GContext *ctx, GRect rect) {
}

size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t id, uint32_t start_bytes,
                                       uint8_t *buffer, size_t num_bytes) {
  if (start_bytes >= sizeof(s_font_data)) {
    return 0;
  }
  num_bytes = MIN(num_bytes, sizeof(s_font_data) - start_bytes);
  memcpy(buffer, &s_font_data[start_bytes], num_bytes);
  return num_bytes;
}

const GlyphData *text_resources_get_glyph(FontCache *font_cache, const Codepoint codepoint,
                                          FontInfo *fontinfo, GlyphLocation *location_out) {
  if (location_out) {
    *location_out = (GlyphLocation){.font_res = &s_font_res};
  }
  return (const GlyphData *)s_font_data;
}

bool text_resources_glyph_is_color(const GlyphLocation *location) {
  return location->font_res == &s_font_res;
}

extern int32_t prv_convert_1bit_addr_to_8bit_x(GBitmap *dest_bitmap, uint32_t *block_addr,
                                               int32_t y_offset);

static int32_t prv_get_8bit_x_from_1bit_x(int32_t dest_1bit_x) {
  return (((dest_1bit_x / 32) * 4)) * 8;
}

void test_text_render__convert_1bit_to_8bit_144x168(void) {
  GSize size = GSize(144, 168);
  const int row_1bit_size_words = 1 + (size.w - 1) / 32;

  GBitmap *bitmap = gbitmap_create_blank(size, GBitmapFormat8Bit);
  uintptr_t base = (uintptr_t)bitmap->addr;

  int dest_x = 0;
  int dest_y = 0;
  uint32_t *block_addr = NULL;

  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 50;
  dest_y = 0;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 0;
  dest_y = 50;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 20;
  dest_y = 100;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  gbitmap_destroy(bitmap);
}

void test_text_render__convert_1bit_to_8bit_180x180(void) {
  GSize size = GSize(180, 180);
  const int row_1bit_size_words = 1 + (size.w - 1) / 32;

  GBitmap *bitmap = gbitmap_create_blank(size, GBitmapFormat8Bit);
  uintptr_t base = (uintptr_t)bitmap->addr;

  int dest_x = 0;
  int dest_y = 0;
  uint32_t *block_addr = NULL;

  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 50;
  dest_y = 0;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 0;
  dest_y = 50;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  dest_x = 20;
  dest_y = 100;
  block_addr = (uint32_t *)(uintptr_t)(((dest_y * row_1bit_size_words) + (dest_x / 32)) * 4);
  cl_assert_equal_i(prv_convert_1bit_addr_to_8bit_x(bitmap, block_addr, dest_y),
                    prv_get_8bit_x_from_1bit_x(dest_x));

  gbitmap_destroy(bitmap);
}

#define RED   GColorRedARGB8
#define BLUE  GColorBlueARGB8
#define WHITE GColorWhiteARGB8

// 3x2 glyph at (1, 1): palette {clear, red, blue}
static void prv_set_glyph(uint8_t encoding, const uint8_t *data, uint8_t data_len) {
  const GlyphHeaderData header =
      {.width_px = 3, .height_px = 2, .left_offset_px = 1, .top_offset_px = 1, .horiz_advance = 5};
  const ColorGlyphHeader body = {.encoding = encoding, .palette_size = 3, .data_len = data_len};
  const uint8_t palette[] = {GColorClearARGB8, RED, BLUE};

  memset(s_font_data, 0, sizeof(s_font_data));
  uint8_t *p = s_font_data;
  memcpy(p, &header, sizeof(header));
  p += sizeof(header);
  memcpy(p, &body, sizeof(body));
  p += sizeof(body);
  if (encoding >> 2 != ColorGlyphModeTinted) {
    memcpy(p, palette, sizeof(palette));
    p += sizeof(palette);
  }
  memcpy(p, data, data_len);
}

static void prv_render(GRect clip) {
  GContext ctx = {
    .draw_state = {.clip_box = clip, .text_color = GColorGreen, .compositing_mode = GCompOpAssign},
  };
  render_glyph(&ctx, 0x1F600, NULL, GRect(0, 0, 10, 10));
}

static uint8_t prv_pixel(int x, int y) {
  return ((uint8_t *)s_dest_bitmap->addr)[y * s_dest_bitmap->row_size_bytes + x];
}

static void prv_assert_row(int y, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e) {
  cl_assert_equal_i(prv_pixel(0, y), a);
  cl_assert_equal_i(prv_pixel(1, y), b);
  cl_assert_equal_i(prv_pixel(2, y), c);
  cl_assert_equal_i(prv_pixel(3, y), d);
  cl_assert_equal_i(prv_pixel(4, y), e);
}

void test_text_render__initialize(void) {
  s_dest_bitmap = gbitmap_create_blank(GSize(5, 4), GBitmapFormat8Bit);
  memset(s_dest_bitmap->addr, WHITE, s_dest_bitmap->row_size_bytes * 4);
}

void test_text_render__cleanup(void) {
  gbitmap_destroy(s_dest_bitmap);
}

void test_text_render__color_glyph_raw(void) {
  // 2 bpp rows: [1 0 2] [2 2 1]
  const uint8_t data[] = {0x48, 0xA4};
  prv_set_glyph(1 | (ColorGlyphModeRaw << 2), data, sizeof(data));
  prv_render(GRect(0, 0, 5, 4));

  prv_assert_row(0, WHITE, WHITE, WHITE, WHITE, WHITE);
  prv_assert_row(1, WHITE, RED, WHITE, BLUE, WHITE);
  prv_assert_row(2, WHITE, BLUE, BLUE, RED, WHITE);
  prv_assert_row(3, WHITE, WHITE, WHITE, WHITE, WHITE);
}

void test_text_render__color_glyph_rle(void) {
  // runs: red x2, clear x2, blue x2 (the clear run crosses rows)
  const uint8_t data[] = {0x11, 0x10, 0x12};
  prv_set_glyph(1 | (ColorGlyphModeRle << 2), data, sizeof(data));
  prv_render(GRect(0, 0, 5, 4));

  prv_assert_row(1, WHITE, RED, RED, WHITE, WHITE);
  prv_assert_row(2, WHITE, WHITE, BLUE, BLUE, WHITE);
}

void test_text_render__color_glyph_tinted(void) {
  // 1 bpp rows: [1 0 1] [0 1 0], drawn in the text color
  const uint8_t data[] = {0xA0, 0x40};
  prv_set_glyph(0 | (ColorGlyphModeTinted << 2), data, sizeof(data));
  prv_render(GRect(0, 0, 5, 4));

  prv_assert_row(1, WHITE, GColorGreenARGB8, WHITE, GColorGreenARGB8, WHITE);
  prv_assert_row(2, WHITE, WHITE, GColorGreenARGB8, WHITE, WHITE);
}

void test_text_render__color_glyph_clipped(void) {
  const uint8_t data[] = {0x48, 0xA4};
  prv_set_glyph(1 | (ColorGlyphModeRaw << 2), data, sizeof(data));
  prv_render(GRect(2, 2, 3, 2));

  prv_assert_row(1, WHITE, WHITE, WHITE, WHITE, WHITE);
  prv_assert_row(2, WHITE, WHITE, BLUE, RED, WHITE);
}

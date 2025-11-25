/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"

#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_graphics_line.h"
#include "stubs_heap.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"

const int FrameBuffer_MaxX = 144;
const int FrameBuffer_MaxY = 85;

// stubs
bool gbitmap_init_with_png_data(GBitmap *bitmap, const uint8_t *data, size_t data_size){
  return false;
}
bool gbitmap_png_data_is_png(const uint8_t *data, size_t data_size){
  return false;
}

void cos_lookup(){}
void sin_lookup(){}
void framebuffer_dirty_all(){}
void framebuffer_mark_dirty_rect(){}
void graphics_circle_draw_quadrant(GContext* ctx, int x0, int y0, uint16_t radius,
    GCornerMask quadrant){}
void graphics_circle_quadrant_draw_1px_non_aa(GContext* ctx, GPoint p,
    uint16_t radius, GCornerMask quadrant) {}
void graphics_internal_circle_quadrant_fill_aa(GContext* ctx, GPoint p,
                                               uint16_t radius, GCornerMask quadrant) {}
void graphics_circle_quadrant_draw(GContext* ctx, GPoint p, uint16_t radius,
                                   GCornerMask quadrant) {}
void graphics_circle_quadrant_fill_non_aa(GContext* ctx, GPoint p,
                                          uint16_t radius, GCornerMask quadrant) {}
void sys_get_current_resource_num(){}
void sys_resource_read_only_bytes(){}
void sys_resource_load_range(){}
void sys_resource_size(){}
int32_t integer_sqrt(int64_t x){ return 0;}


GBitmap framebuffer_get_as_bitmap(FrameBuffer *fb, const GSize *size) {
  return (GBitmap) {
    .addr = fb,
    .row_size_bytes = FrameBuffer_MaxX,
    .info = (BitmapInfo) {.format = GBitmapFormat8Bit, .version = GBITMAP_VERSION_CURRENT},
    .bounds = (GRect) { { 0, 0 }, { FrameBuffer_MaxX, FrameBuffer_MaxY } },
  };
}

// tests
void test_graphics_blending__closest_opaque(void) {
  cl_assert_equal_i(GColorRedARGB8, gcolor_closest_opaque((GColor8){.a= 3, .r = 3}).argb);
  cl_assert_equal_i(GColorRedARGB8, gcolor_closest_opaque((GColor8){.a= 2, .r = 3}).argb);
  cl_assert_equal_i(GColorClearARGB8, gcolor_closest_opaque((GColor8){.a= 1, .r = 3}).argb);
  cl_assert_equal_i(GColorClearARGB8, gcolor_closest_opaque((GColor8){.a= 0, .r = 3}).argb);
}

void test_graphics_blending__ctx_text_color_discards_alpha(void) {
  GContext ctx = {};
  graphics_context_set_text_color(&ctx, (GColor8){.a= 3, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.text_color.argb);
  graphics_context_set_text_color(&ctx, (GColor8){.a= 2, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.text_color.argb);
  graphics_context_set_text_color(&ctx, (GColor8){.a= 1, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.text_color.argb);
  graphics_context_set_text_color(&ctx, (GColor8){.a= 0, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.text_color.argb);
}

void test_graphics_blending__ctx_stroke_color_discards_alpha(void) {
  GContext ctx = {};
  graphics_context_set_stroke_color(&ctx, (GColor8){.a= 3, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.stroke_color.argb);
  graphics_context_set_stroke_color(&ctx, (GColor8){.a= 2, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.stroke_color.argb);
  graphics_context_set_stroke_color(&ctx, (GColor8){.a= 1, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.stroke_color.argb);
  graphics_context_set_stroke_color(&ctx, (GColor8){.a= 0, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.stroke_color.argb);
}

void test_graphics_blending__ctx_fill_color_discards_alpha(void) {
  GContext ctx = {};
  graphics_context_set_fill_color(&ctx, (GColor8){.a= 3, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.fill_color.argb);
  graphics_context_set_fill_color(&ctx, (GColor8){.a= 2, .r = 3});
  cl_assert_equal_i(GColorRedARGB8, ctx.draw_state.fill_color.argb);
  graphics_context_set_fill_color(&ctx, (GColor8){.a= 1, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.fill_color.argb);
  graphics_context_set_fill_color(&ctx, (GColor8){.a= 0, .r = 3});
  cl_assert_equal_i(GColorClearARGB8, ctx.draw_state.fill_color.argb);
}

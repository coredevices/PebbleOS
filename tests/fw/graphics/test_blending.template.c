/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/bitblt_private.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"

#include "util/math.h"
#include "util.h"

#include "clar.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "${BIT_DEPTH_NAME}/test_framebuffer.h"

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

// Tests
////////////////////////////////////

#define ORIGIN_RECT_NO_CLIP GRect(0, 0, 144, 168)
#define OFFSET_X 9
#define OFFSET_Y 20
#define STRIPE_WIDTH 64
#define STRIPE_HEIGHT 1

void prv_test_plot_horizontal_line(GBitmap *framebuffer, GRect area, GColor color) {
  int y = area.origin.y;
  int x0 = area.origin.x;
  int x1 = x0 + area.size.w;

  if (y >= framebuffer->bounds.size.h || y < 0) {
    return;
  }

  int16_t x_min = MAX(MIN(x0, x1), framebuffer->bounds.origin.x);
  int16_t x_max = MIN(MAX(x0, x1), framebuffer->bounds.origin.x + framebuffer->bounds.size.w);

  for (int i=x_min; i < x_max; i++) {
#if SCREEN_COLOR_DEPTH_BITS == 8
    GColor *output = (GColor*)framebuffer->addr + y * framebuffer->row_size_bytes + i;
    //                                     src_col, dest_col
    output->argb = gcolor_alpha_blend(color, (*output)).argb;
#endif // SCREEN_COLOR_DEPTH_BITS == 8
  }
}

void test_blending_${BIT_DEPTH_NAME}__photoshop(void) {

  GContext ctx;
  FrameBuffer *fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) { DISP_COLS, DISP_ROWS });
  test_graphics_context_init(&ctx, fb);
  GBitmap *background_0_100 = get_gbitmap_from_pbi("blendtest_0_100_backdrop.pbi");
  GBitmap *background_33_66 = get_gbitmap_from_pbi("blendtest_33_66_backdrop.pbi");

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_bitmap_in_rect(&ctx, background_0_100, &ORIGIN_RECT_NO_CLIP);

#if SCREEN_COLOR_DEPTH_BITS == 8

  // Sanity check
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "blendtest_0_100_backdrop.pbi"));

  // 0% alpha channel blend test
  uint8_t color = 0x0;
  uint8_t alpha = 0x0;

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_bitmap_in_rect(&ctx, background_0_100, &ORIGIN_RECT_NO_CLIP);

  for (int i=0; i < 64; i++) {
    prv_test_plot_horizontal_line(&ctx.dest_bitmap,
                                  GRect(OFFSET_X, OFFSET_Y + i, STRIPE_WIDTH, STRIPE_HEIGHT),
                                  (GColor){.argb = color | alpha});
    color++;
  }

  // 100% alpha channel blend test
  color = 0x0;
  alpha = 0xC0;

  for (int i=64; i < 128; i++) {
    prv_test_plot_horizontal_line(&ctx.dest_bitmap,
                                  GRect(OFFSET_X, OFFSET_Y + i, STRIPE_WIDTH, STRIPE_HEIGHT),
                                  (GColor){.argb = color | alpha});
    color++;
  }

  // Compare the results
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "blendtest_0_100.pbi"));

  // Reset canvas
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_bitmap_in_rect(&ctx, background_33_66, &ORIGIN_RECT_NO_CLIP);

  // 33% alpha channel blend test
  color = 0x0;
  alpha = 0x40;

  for (int i=0; i < 64; i++) {
    prv_test_plot_horizontal_line(&ctx.dest_bitmap,
                                  GRect(OFFSET_X, OFFSET_Y + i, STRIPE_WIDTH, STRIPE_HEIGHT),
                                  (GColor){.argb = color | alpha});
    color++;
  }

  // 66% alpha channel blend test
  color = 0x0;
  alpha = 0x80;

  for (int i=64; i < 128; i++) {
    prv_test_plot_horizontal_line(&ctx.dest_bitmap,
                                  GRect(OFFSET_X, OFFSET_Y + i, STRIPE_WIDTH, STRIPE_HEIGHT),
                                  (GColor){.argb = color | alpha});
    color++;
  }

  // Compare the results
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "blendtest_33_66.pbi"));
#endif


  gbitmap_destroy(background_0_100);
  gbitmap_destroy(background_33_66);
  free(fb);
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "util/trig.h"

#include "applib/ui/layer.h"
#include "applib/ui/window_private.h"


#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#if SCREEN_COLOR_DEPTH_BITS == 1
  #include "1bit/test_framebuffer.h"
#else
  #include "8bit/test_framebuffer.h"
#endif

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

///////////////////////////////////////////////////////////
// Fakes
#include "fake_gbitmap_get_data_row.h"

// Setup
////////////////////////////////////
static GBitmap *test_image_bw;
static GBitmap *test_image_color;
static FrameBuffer *fb = NULL;

#if SCREEN_COLOR_DEPTH_BITS == 1
extern bool get_bitmap_bit(GBitmap *bmp, int x, int y);
#elif SCREEN_COLOR_DEPTH_BITS == 8
extern GColor get_bitmap_color(GBitmap *bmp, int x, int y);
#endif

void test_graphics_draw_rotated_bitmap__initialize(void) {
  s_fake_data_row_handling = false;
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_image_bw = get_gbitmap_from_pbi("test_rotated_bitmap_no_litter.Xbit.pbi");
  cl_assert(test_image_bw != NULL);

  test_image_color = get_gbitmap_from_pbi("test_rotated_bitmap_redstar.Xbit.pbi");
  cl_assert(test_image_color != NULL);
}

void test_graphics_draw_rotated_bitmap__cleanup(void) {
  free(fb);
  if (test_image_bw) {
    if (test_image_bw->addr) {
      free(test_image_bw->addr);
    }
    free(test_image_bw);
  }

  if (test_image_color) {
    if (test_image_color->addr) {
      free(test_image_color->addr);
    }
    free(test_image_color);
  }
}

static void setup_test_rotate_bitmap(GContext *ctx, FrameBuffer *fb,
                                     GRect clip_box, GRect drawing_box,
                                     GCompOp compositing_mode) {
  test_graphics_context_reset(ctx, fb);

  GDrawState draw_state = {
    .clip_box = clip_box,
    .drawing_box = drawing_box,
    .compositing_mode = compositing_mode,
  };
  setup_test_context(ctx, 
                     (CTX_FLAG_DS_CLIP_BOX | CTX_FLAG_DS_DRAWING_BOX |
                      CTX_FLAG_DS_COMPOSITING_MODE),
                     &draw_state, NULL);
}

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, DISP_COLS, DISP_ROWS)

// Tests
////////////////////////////////////
void test_graphics_draw_rotated_bitmap__get_color(void) {
#if SCREEN_COLOR_DEPTH_BITS == 1
  cl_check(get_bitmap_bit(test_image_bw, 8, 16) == 1);
  cl_check(get_bitmap_bit(test_image_bw, 8, 24) == 0);
  cl_check(get_bitmap_bit(test_image_color, 30, 2) == 0);
  cl_check(get_bitmap_bit(test_image_color, 30, 10) == 0);
  cl_check(get_bitmap_bit(test_image_color, 30, 30) == 1);
#elif SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gcolor_equal(get_bitmap_color(test_image_bw, 8, 16), GColorWhite));
  cl_check(gcolor_equal(get_bitmap_color(test_image_bw, 8, 24), GColorBlack));
  cl_check(gcolor_equal(get_bitmap_color(test_image_color, 30, 2), GColorClear));
  cl_check(gcolor_equal(get_bitmap_color(test_image_color, 30, 10), GColorRed));
  cl_check(gcolor_equal(get_bitmap_color(test_image_color, 30, 30), GColorScreaminGreen));
#endif
}


void test_graphics_draw_rotated_bitmap__origin_bw_assign(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_origin_bw_assign_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          "draw_rotated_bitmap_origin_bw_assign_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPoint(27, 40), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          "draw_rotated_bitmap_origin_bw_assign_center_45.Xbit.pbi"));

}

void test_graphics_draw_rotated_bitmap__origin_bw_set(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_origin_bw_set_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_bw_set_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPoint(27, 40), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_bw_set_center_45.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__offset_bw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0, Offset
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_bw_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_bw_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_bw, 
                               GPoint(27, 40), DEG_TO_TRIGANGLE(45), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_bw_center_45.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__origin_color_assign(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_origin_color_assign_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_color_assign_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPoint(30, 30), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_color_assign_center_45.Xbit.pbi"));

  // Test transparency
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_context_set_fill_color(&ctx, GColorBlue);
  graphics_fill_rect(&ctx, &GRect(0, 0, 20, 10));
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPoint(30, 30), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(
           &ctx.dest_bitmap, 
           "draw_rotated_bitmap_origin_color_assign_center_45_transparent.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__origin_color_set(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_origin_color_set_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_color_set_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPoint(30, 30), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
           "draw_rotated_bitmap_origin_color_set_center_45.Xbit.pbi"));

  // Test transparency
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpSet);
  graphics_context_set_fill_color(&ctx, GColorBlue);
  graphics_fill_rect(&ctx, &GRect(0, 0, 20, 10));
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPoint(30, 30), DEG_TO_TRIGANGLE(45), GPointZero);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, 
                          "draw_rotated_bitmap_origin_color_set_center_45_transparent.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__offset_color(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(0), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_color_0.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPointZero, DEG_TO_TRIGANGLE(45), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_color_corner_45.Xbit.pbi"));

  // Top-left center rotation point, Angle 45
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color, 
                               GPoint(30, 30), DEG_TO_TRIGANGLE(45), GPoint(20, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_color_center_45.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__offset_edge(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // bottom edge center rotation point, Angle 2
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color,
                               GPoint(30, 59), DEG_TO_TRIGANGLE(2), GPoint(72, 84));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_bottomedge_2.Xbit.pbi"));

  // top edge center rotation point, Angle 2
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color,
                               GPoint(30, 1), DEG_TO_TRIGANGLE(2), GPoint(72, 84));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_topedge_2.Xbit.pbi"));

  // left edge center rotation point, Angle 2
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color,
                               GPoint(1, 30), DEG_TO_TRIGANGLE(2), GPoint(72, 84));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_leftedge_2.Xbit.pbi"));

  // right edge center rotation point, Angle 2
  setup_test_rotate_bitmap(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(&ctx, test_image_color,
                               GPoint(59, 30), DEG_TO_TRIGANGLE(2), GPoint(72, 84));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rotated_bitmap_offset_rightedge_2.Xbit.pbi"));
}

void test_graphics_draw_rotated_bitmap__data_row_handling(void) {
  // Enable fake data row handling which will override the gbitmap_get_data_row_xxx() functions
  // with their fake counterparts in fake_gbitmap_get_data_row.c
  s_fake_data_row_handling = true;
  s_fake_data_row_handling_disable_vertical_flip = true;

  GContext *ctx = malloc(sizeof(GContext));
  test_graphics_context_init(ctx, fb);
  framebuffer_clear(fb);

  GBitmap *test_image = get_gbitmap_from_pbi("stamp.Xbit.pbi");
  cl_assert(test_image != NULL);

  // PBL-24705 grect_center_point is off by 1
  GPoint center = GPoint(DISP_COLS / 2 - 1, DISP_ROWS / 2 - 1);

  // No Clip, Angle 0
  setup_test_rotate_bitmap(ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, test_image, GPoint(
                               test_image->bounds.size.w / 2 - 1, 
                               test_image->bounds.size.h / 2 - 1), 
                               DEG_TO_TRIGANGLE(0), center);
  cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, "draw_rotated_bitmap_stamp_0deg.Xbit.pbi"));

  // Top-left corner rotation point, Angle 45
  setup_test_rotate_bitmap(ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, test_image, 
                               GPoint(71, 71), DEG_TO_TRIGANGLE(45), center);
  cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, "draw_rotated_bitmap_stamp_45deg.Xbit.pbi"));
  // Top-left corner rotation point, Angle 180
  setup_test_rotate_bitmap(ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, test_image, 
                               GPoint(71, 71), DEG_TO_TRIGANGLE(180), center);
  cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, "draw_rotated_bitmap_stamp_180deg.Xbit.pbi"));
}

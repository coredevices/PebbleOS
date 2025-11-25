/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"


#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "${BIT_DEPTH_NAME}/test_framebuffer.h"

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_stroke_circle_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_stroke_circle_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

#define RADIUS_BIG 15
#define RADIUS_MEDIUM 8
#define RADIUS_MIN_CALCULATED 3
#define RADIUS_MAX_PRECOMPUTED 2
#define RADIUS_SMALL 1
#define RADIUS_NONE 0
#define STROKE_BIG 10
#define STROKE_SMALL 5
#define STROKE_THREE 3  // In case of fluctuation

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 40, 50)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 30, 40)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define CENTER_OF_ORIGIN_RECT      GPoint(20, 25)
#define CENTER_OF_ORIGIN_RECT_NXNY GPoint(10, 15)

void test_graphics_stroke_circle_${BIT_DEPTH_NAME}__origin_layer(void) {

#if SCREEN_COLOR_DEPTH_BITS == 8

  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Big circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r16_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r16_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r16_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Medium circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r8_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r8_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r8_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Small circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Testing of the special cases for radius:

  // Radius of 3 - starting point for calculated edges
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_THREE);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MIN_CALCULATED);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Radius of 2 - ending point for precomputed edges
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_THREE);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MAX_PRECOMPUTED);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // No circle
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_THREE);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_NONE);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_origin_aa_r0_no_clip.${BIT_DEPTH_NAME}.pbi"));

#endif // SCREEN_COLOR_DEPTH_BITS == 8

}

#define OFFSET_RECT_NO_CLIP        GRect(10, 10, 40, 50)
#define OFFSET_RECT_CLIP_XY        GRect(10, 10, 30, 40)
#define OFFSET_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define CENTER_OF_OFFSET_RECT      GPoint(10, 15)
#define CENTER_OF_OFFSET_RECT_NXNY GPoint(0, 5)

void test_graphics_stroke_circle_${BIT_DEPTH_NAME}__offset_layer_aa(void) {

#if SCREEN_COLOR_DEPTH_BITS == 8

  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Big circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r16_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r16_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, STROKE_BIG);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r16_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Medium circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Small circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, STROKE_SMALL);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif // SCREEN_COLOR_DEPTH_BITS == 8

}

extern void graphics_circle_quadrant_draw_stroked_non_aa(
    GContext* ctx, GPoint p, uint16_t radius, uint8_t stroke_width,
    GCornerMask quadrant);

void test_graphics_stroke_circle_${BIT_DEPTH_NAME}__quadrants(void) {

#if SCREEN_COLOR_DEPTH_BITS == 8

  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerTopLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quad_top_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerTopRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quad_top_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerBottomLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quad_bottom_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerBottomRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quad_bottom_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersTop);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quads_top.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersBottom);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quads_bottom.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quads_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_r8_quads_left.${BIT_DEPTH_NAME}.pbi"));

#endif // SCREEN_COLOR_DEPTH_BITS == 8

}

extern void graphics_circle_quadrant_draw_stroked_aa(
    GContext* ctx, GPoint p, uint16_t radius, uint8_t stroke_width,
    GCornerMask quadrant);

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__quadrants_aa(void) {

#if SCREEN_COLOR_DEPTH_BITS == 8

  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerTopLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quad_top_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerTopRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quad_top_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerBottomLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quad_bottom_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornerBottomRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quad_bottom_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersTop);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quads_top.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersBottom);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quads_bottom.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quads_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_SMALL);
  graphics_circle_quadrant_draw_stroked_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, STROKE_SMALL,GCornersLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "stroke_circle_offset_aa_r8_quads_left.${BIT_DEPTH_NAME}.pbi"));

#endif // SCREEN_COLOR_DEPTH_BITS == 8

}

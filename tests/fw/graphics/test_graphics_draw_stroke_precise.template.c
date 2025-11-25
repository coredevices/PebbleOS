/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"

#include "applib/graphics/bitblt_private.h"

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
void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 30, 40)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define START_ON_ORIGIN_RECT       GPointPrecise(5, 5)
#define END_ON_ORIGIN_RECT         GPointPrecise(25, 25)
#define START_ON_ORIGIN_RECT_XY    GPointPrecise(15, 15)
#define END_ON_ORIGIN_RECT_XY      GPointPrecise(35, 35)
#define START_ON_ORIGIN_RECT_NXNY  GPointPrecise(-5, -5)
#define END_ON_ORIGIN_RECT_NXNY    GPointPrecise(15, 15)

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__origin_layer_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if PBL_COLOR
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_origin_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_x_origin_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_nxny_origin_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_point_origin_layer_aa.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__origin_layer_non_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_origin_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_x_origin_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_nxny_origin_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_point_origin_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));
}

#define OFFSET_RECT_NO_CLIP        GRect(10, 10, 144, 168)
#define OFFSET_RECT_CLIP_XY        GRect(10, 10, 30, 40)
#define OFFSET_RECT_CLIP_NXNY      GRect(10, 10, 30, 40)

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__offset_layer_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if PBL_COLOR
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_offset_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_x_offset_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_nxny_offset_layer_aa.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_point_offset_layer_aa.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__offset_layer_non_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_offset_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_x_offset_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_across_nxny_offset_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_point_offset_layer_non_aa.${BIT_DEPTH_NAME}.pbi"));
}

#define COLOR_START_POINT   GPointPrecise(5, 35)
#define COLOR_END_POINT     GPointPrecise(45, 40)

void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__color(void) {
  // TODO: Fix blending and reenable this - PBL-16509
/*
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_context_set_stroke_color(&ctx, GColorBlack);
  graphics_line_draw_precise_stroked_aa(&ctx, COLOR_START_POINT, COLOR_END_POINT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_context_set_stroke_color(&ctx, GColorClear);
  graphics_line_draw_precise_stroked_non_aa(&ctx, COLOR_START_POINT, COLOR_END_POINT, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
*/
}

/*
 * Following points come from bug reports, causing "plasma effect" where multiple
 *   lines in close vicinity of one spot (~1 pixel) caused artifact instead of
 *   elegant AA circle.
 */

// First pair, distance less than 1px
#define CLOSE_POINTS_LESS_THAN_1PX_START     (GPointPrecise){{.integer = 71, .fraction = 4}, {.integer = 73, .fraction = 5}}
#define CLOSE_POINTS_LESS_THAN_1PX_END       (GPointPrecise){{.integer = 71, .fraction = 5}, {.integer = 73, .fraction = 6}}
//Second pair, distance around 1px
#define CLOSE_POINTS_AROUND_1PX_START        (GPointPrecise){{.integer = 71, .fraction = 4}, {.integer = 74, .fraction = 1}}
#define CLOSE_POINTS_AROUND_1PX_END          (GPointPrecise){{.integer = 71, .fraction = 1}, {.integer = 73, .fraction = 3}}

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__close_points_aa(void) {
#if PBL_COLOR
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, CLOSE_POINTS_LESS_THAN_1PX_START, CLOSE_POINTS_LESS_THAN_1PX_END, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_close_points_less_than_1px_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 10);
  graphics_line_draw_precise_stroked_aa(&ctx, CLOSE_POINTS_AROUND_1PX_START, CLOSE_POINTS_AROUND_1PX_END, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_close_points_around_1px_aa.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__close_points_non_aa(void) {

  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, CLOSE_POINTS_LESS_THAN_1PX_START, CLOSE_POINTS_LESS_THAN_1PX_END, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_close_points_less_than_1px_non_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 10);
  graphics_line_draw_precise_stroked_non_aa(&ctx, CLOSE_POINTS_AROUND_1PX_START, CLOSE_POINTS_AROUND_1PX_END, 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_close_points_around_1px_non_aa.${BIT_DEPTH_NAME}.pbi"));
}

/*
 * Following functions will test issue of same starting/ending point for stroke width, where point lies
 *   between pixels due to precise points. This should be fixed by PBL-20783.
 */
void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__same_point_aa(void) {
#if PBL_COLOR
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 10);
  int radius = 5;
  int x_offset = 10 * FIXED_S16_3_ONE.raw_value;
  for (int i = 0; i < 8; i++) {
    radius += 1;
    x_offset += radius * FIXED_S16_3_ONE.raw_value + 4 * FIXED_S16_3_ONE.raw_value;
    GPointPrecise p = GPointPrecise(x_offset, 15 * FIXED_S16_3_ONE.raw_value);
    for (int j=0; j<9; j++) {
      graphics_line_draw_precise_stroked_aa(&ctx, p, p, radius);
      p.x.raw_value += 1;
      p.y.integer += 16;
    }
  }

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_points_pattern_aa.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_draw_stroke_precise_${BIT_DEPTH_NAME}__same_point_non_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 10);
  int radius = 5;
  int x_offset = 10 * FIXED_S16_3_ONE.raw_value;
  for (int i = 0; i < 8; i++) {
    radius += 1;
    x_offset += radius * FIXED_S16_3_ONE.raw_value + 4 * FIXED_S16_3_ONE.raw_value;
    GPointPrecise p = GPointPrecise(x_offset, 15 * FIXED_S16_3_ONE.raw_value);
    for (int j=0; j<9; j++) {
      graphics_line_draw_precise_stroked_non_aa(&ctx, p, p, radius);
      p.x.raw_value += 1;
      p.y.integer += 16;
    }
  }

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_precise_same_points_pattern_non_aa.${BIT_DEPTH_NAME}.pbi"));
}

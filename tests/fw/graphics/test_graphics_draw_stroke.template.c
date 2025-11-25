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
void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(50, 40), GPoint(70, 35));
}

void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(-25, 35), GPoint(15, 40));
}

void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(40, 50), GPoint(35, 70));
}

void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, -30), GPoint(45, 30));
}

void across_screen_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(25, 25), GPoint(119, 143));
}

void same_start_stop_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(20, 20), GPoint(20, 20));
}

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 30, 40)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define START_ON_ORIGIN_RECT       GPoint(5, 5)
#define END_ON_ORIGIN_RECT       GPoint(25, 25)
#define START_ON_ORIGIN_RECT_XY    GPoint(15, 15)
#define END_ON_ORIGIN_RECT_XY      GPoint(35, 35)
#define START_ON_ORIGIN_RECT_NXNY  GPoint(-5, -5)
#define END_ON_ORIGIN_RECT_NXNY    GPoint(15, 15)
#define STROKE_WIDTH               10

void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_across_nxny_origin_layer.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_same_point_origin_layer.${BIT_DEPTH_NAME}.pbi"));
}

#define OFFSET_RECT_NO_CLIP        GRect(10, 10, 144, 168)
#define OFFSET_RECT_CLIP_XY        GRect(10, 10, 30, 40)
#define OFFSET_RECT_CLIP_NXNY      GRect(10, 10, 30, 40)

void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT_XY, END_ON_ORIGIN_RECT_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_across_x_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, START_ON_ORIGIN_RECT_NXNY, END_ON_ORIGIN_RECT_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_across_nxny_offset_layer.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, STROKE_WIDTH);
  graphics_draw_line(&ctx, END_ON_ORIGIN_RECT, END_ON_ORIGIN_RECT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_same_point_offset_layer.${BIT_DEPTH_NAME}.pbi"));
}

#define COLOR_START_POINT GPoint(5, 35)
#define COLOR_END_POINT GPoint(45, 40)

void test_graphics_draw_stroke_${BIT_DEPTH_NAME}__color(void) {
  // TODO: Fix blending and reenable this - PBL-16509
/*
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_context_set_stroke_color(&ctx, GColorBlack);
  graphics_draw_line(&ctx, COLOR_START_POINT, COLOR_END_POINT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 10);
  graphics_context_set_stroke_color(&ctx, GColorClear);
  graphics_draw_line(&ctx, COLOR_START_POINT, COLOR_END_POINT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_stroke_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
*/
}

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
void test_graphics_draw_rect_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_rect_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

////////////////////////////////////

static void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(4, 2, 16, 8));
}

static void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, &GRect(4, 2, 16, 8));
}

static void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_draw_rect(ctx, &GRect(4, 2, 16, 8));
}

static void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(10, 2, 18, 4));
}

static void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(-10, 2, 18, 4));
}

static void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(4, 5, 18, 10));
}

static void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(4, -5, 18, 10));
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 20, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_nx_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_y_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_ny_origin_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 15, 20, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_x_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_nx_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_y_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_across_ny_offset_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__color(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 20, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
}

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 20, 20)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 144, 168)
#define ORIGIN_DRAW_RECT_NO_CLIP   GRect(6, 6, 30, 40)
#define ORIGIN_DRAW_RECT_CLIP_XY   GRect(6, 6, 30, 40)
#define ORIGIN_DRAW_RECT_CLIP_NXNY GRect(-16, -16, 30, 40)

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__origin_aa_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // AA = true, SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 2
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 3
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // AA = true, SW = 4
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

  // AA = true, SW = 5
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // AA = true, SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_aa_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__origin_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 1);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 2
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 2);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 3
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 3);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // SW = 4
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 4);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

  // SW = 5
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 5);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 11);
  graphics_draw_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_origin_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

}

#define OFFSET_RECT_NO_CLIP        GRect(20, 10, 144, 168)
#define OFFSET_RECT_CLIP_XY        GRect(20, 10, 20, 20)
#define OFFSET_RECT_CLIP_NXNY      GRect(20, 10, 144, 168)
#define OFFSET_DRAW_RECT_NO_CLIP   GRect(6, 6, 30, 40)
#define OFFSET_DRAW_RECT_CLIP_XY   GRect(6, 6, 30, 40)
#define OFFSET_DRAW_RECT_CLIP_NXNY GRect(-16, -16, 30, 40)

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__offset_aa_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // AA = true, SW = 1
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 2
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 3
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // AA = true, SW = 4
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

  // AA = true, SW = 5
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // AA = true, SW = 11
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_aa_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__offset_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // SW = 1
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 1);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 2
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 2);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 3
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 3);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // SW = 4
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 4);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

  // SW = 5
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 5);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8

  // SW = 11
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 11);
  graphics_draw_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_offset_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

#endif

}

#define BOX_SIZE 8
#define CLIP_RECT_DRAW_BOX GRect(10, 10, 140, 30)
#define CLIP_RECT_CLIP_BOX GRect(10, 10, 120, 2*BOX_SIZE + 4)
#define CLIP_RECT_RECT_BOX GRect(0, 0, BOX_SIZE, BOX_SIZE)
#define CLIP_OFFSET 40
// yoffset if used to allow for multiple test cases to be drawn and tested on the same test image
// nudge is used to move the drawn rectangle just outside each corner the clipping
static void prv_draw_rects(GContext *ctx, uint8_t sw, uint16_t yoffset, int16_t nudge) {
  // Adjust drawing box and clipping box
  ctx->draw_state.drawing_box = CLIP_RECT_DRAW_BOX;
  ctx->draw_state.drawing_box.origin.y += yoffset;
  ctx->draw_state.clip_box = CLIP_RECT_CLIP_BOX;
  ctx->draw_state.clip_box.origin.y += yoffset;
  graphics_context_set_stroke_width(ctx, sw);

  // Top left corner of clipping box
  GRect box = CLIP_RECT_RECT_BOX;
  box.origin.x -= nudge;
  box.origin.y -= nudge;
  graphics_draw_rect(ctx, &box);

  // Top right corner of clipping box
  box.origin.x = CLIP_RECT_CLIP_BOX.size.w - BOX_SIZE;
  box.origin.x += nudge;
  graphics_draw_rect(ctx, &box);

  // Bottom right corner of clipping box
  box.origin.y = CLIP_RECT_CLIP_BOX.size.h - BOX_SIZE;
  box.origin.y += nudge;
  graphics_draw_rect(ctx, &box);

  // Bottom left corner of clipping box
  box.origin.x = 0;
  box.origin.x -= nudge;
  graphics_draw_rect(ctx, &box);
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__clipping_rect(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Draw rectangles around boundaries of clipping box - AA false
  prv_draw_rects(&ctx, 1, 0, 0);
  prv_draw_rects(&ctx, 2, CLIP_OFFSET, 0);
  prv_draw_rects(&ctx, 3, 2 * CLIP_OFFSET, 0);
  prv_draw_rects(&ctx, 4, 3 * CLIP_OFFSET, 0);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_clip_rect.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);
  prv_draw_rects(&ctx, 1, 0, 1);
  prv_draw_rects(&ctx, 2, CLIP_OFFSET, 1);
  prv_draw_rects(&ctx, 3, 2 * CLIP_OFFSET, 1);
  prv_draw_rects(&ctx, 4, 3 * CLIP_OFFSET, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_clip_rect_nudge.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__clipping_rect_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, true, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Draw rectangles around boundaries of clipping box - AA true
  prv_draw_rects(&ctx, 1, 0, 0);
  prv_draw_rects(&ctx, 2, CLIP_OFFSET, 0);
  prv_draw_rects(&ctx, 3, 2 * CLIP_OFFSET, 0);
  prv_draw_rects(&ctx, 4, 3 * CLIP_OFFSET, 0);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_clip_rect_aa.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, true, 1);
  prv_draw_rects(&ctx, 1, 0, 1);
  prv_draw_rects(&ctx, 2, CLIP_OFFSET, 1);
  prv_draw_rects(&ctx, 3, 2 * CLIP_OFFSET, 1);
  prv_draw_rects(&ctx, 4, 3 * CLIP_OFFSET, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_rect_clip_rect_aa_nudge.${BIT_DEPTH_NAME}.pbi"));
}

#define ORIGIN_RECT                GRect(0, 0, 144, 4)
#define LEFT_EDGE_RECT             GRect(5, 4, 59, 4)
#define RIGHT_EDGE_RECT            GRect(0, 8, 40, 4)
#define BOTH_EDGE_RECT             GRect(5, 12, 20, 4)
#define CLIPPED_RECT               GRect(-10,16,20,4)
#define OVERLAP_RECT               GRect(0,0,20,20)
#define CORNER_RADIUS_RECT         GRect(5,24,20,20)

static void prv_draw_dither_rects(GContext* ctx, GColor8 fill_color) {
  test_graphics_context_init(ctx, fb);
  graphics_context_set_fill_color(ctx, fill_color);

  graphics_fill_rect(ctx, &ORIGIN_RECT);
  graphics_fill_rect(ctx, &LEFT_EDGE_RECT);
  graphics_fill_rect(ctx, &RIGHT_EDGE_RECT);
  graphics_fill_rect(ctx, &BOTH_EDGE_RECT);
  graphics_fill_rect(ctx, &CLIPPED_RECT);
  graphics_fill_round_rect(ctx, &CORNER_RADIUS_RECT, 4, GCornersAll);
  ctx->draw_state.drawing_box = GRect(100,2, 40, 40);
  graphics_fill_rect(ctx, &OVERLAP_RECT);

  cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, "draw_multiple_rect_dithered.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_rect_${BIT_DEPTH_NAME}__dithering_gray(void) {
#if SCREEN_COLOR_DEPTH_BITS == 1
  GContext ctx;
  prv_draw_dither_rects(&ctx, GColorLightGray);
  prv_draw_dither_rects(&ctx, GColorDarkGray);
#endif
}


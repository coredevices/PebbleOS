/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

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

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_fill_rect_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_fill_rect_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &GRect(4, 2, 16, 8));
}

void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(10, 2, 18, 4), 4, GCornersAll);
}

void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(-10, 2, 18, 4), 4, GCornersAll);
}

void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(4, 5, 18, 10), 4, GCornersAll);
}

void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(4, -5, 18, 10), 4, GCornersAll);
}

void corners_all_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(2, 2, 28, 20), 4, GCornersAll);
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_round_rect(ctx, &GRect(2, 2, 28, 20), 4, GCornersAll);
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_fill_round_rect(ctx, &GRect(2, 2, 28, 20), 4, GCornersAll);
}

void corners_clipped_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &GRect(-19, 0, 20, 20), 4, GCornersAll);
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 20, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_nx_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_y_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_ny_origin_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 15, 20, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_x_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_nx_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_y_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_across_ny_offset_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__clipped(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 100, 100));
  layer_set_update_proc(&layer, &corners_clipped_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_clipped.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__color(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 32, 24));
  layer_set_update_proc(&layer, &corners_all_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_corners_all.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &corners_all_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_corners_all.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
}

#define RECT_WIDTH                 30
#define RECT_HEIGHT                40
#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 20, 20)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 144, 168)
#define ORIGIN_DRAW_RECT_NO_CLIP   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define ORIGIN_DRAW_RECT_CLIP_XY   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define ORIGIN_DRAW_RECT_CLIP_NXNY GRect(-16, -16, RECT_WIDTH, RECT_HEIGHT)

void test_graphics_fill_rect_8bit__transparency(void) {
  GContext ctx;
  FrameBuffer fb;
  framebuffer_init(&fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&ctx, &fb);

  // AA = true, SW has no effect, all corners
  setup_test_aa_sw(&ctx, &fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_context_set_fill_color(&ctx, GColorBlue);
  graphics_fill_rect(&ctx, &GRect(10, 10, 100, 140));
  graphics_context_set_fill_color(&ctx, GColorRed);
  ctx.draw_state.fill_color.a = 0;
  graphics_fill_rect(&ctx, &GRect(20, 20, 70, 20));
  ctx.draw_state.fill_color.a = 1;
  graphics_fill_rect(&ctx, &GRect(20, 50, 70, 20));
  ctx.draw_state.fill_color.a = 2;
  graphics_fill_rect(&ctx, &GRect(20, 80, 70, 20));
  ctx.draw_state.fill_color.a = 3;
  graphics_fill_rect(&ctx, &GRect(20, 110, 70, 20));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_transparent.8bit.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__origin_radius(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // AA = true, SW has no effect, all corners
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r0_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 4, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 5, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 6, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r6_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 7, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r7_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 8, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r8_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should draw same as radius 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 9, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r9_no_clip.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__origin_radius_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // AA = true, SW has no effect, all corners
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r0_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r1_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r2_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

// TODO: Remove these #ifdefs in PBL-15916 when support for non-antialiased rounded rect
#if SCREEN_COLOR_DEPTH_BITS == 8
  // TODO: Fix left corners PBL-15915
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r3_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix left corners PBL-15915
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 4, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r4_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix left corners PBL-15915
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 5, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r5_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix left corners PBL-15915
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 6, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r6_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix left corners PBL-15915
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 7, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r7_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 8, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r8_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

// TODO: Remove these #ifdefs in PBL-15916 when support for non-antialiased rounded rect
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 9, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r9_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2), GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax1_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should draw same as rmax1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax2_aa_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  // AA = true, SW has no effect, Clip XY, all corners
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, 0, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r0_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

// TODO: Remove these #ifdefs in PBL-15916 when support for non-antialiased rounded rect
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r1_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, 2, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r2_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, 3, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r3_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2), GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax1_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  // Should draw same as rmax1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax2_aa_clip_xy.${BIT_DEPTH_NAME}.pbi"));
#endif

  // AA = true, SW has no effect, Clip NXNY, all corners
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , 0, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r0_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

// TODO: Remove these #ifdefs in PBL-15916 when support for non-antialiased rounded rect
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r1_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , 2, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r2_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , 3, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_r3_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2), GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax1_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Should draw same as rmax1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY , ORIGIN_RECT_CLIP_NXNY , true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY , ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1, GCornersAll);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax2_aa_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__origin_corners(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Currently prv_fill_rect only supports up to radius 8, fix in PBL-15916 to support
  // arbitrary radius values - this affects all tests here
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersBottom);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_bottom.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersTop);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_top.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerTopLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_topleft.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerTopRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_topright.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerBottomLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_bottomleft.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerBottomRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_bottomright.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__origin_aa_corners(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

// TODO: Remove these #ifdefs in PBL-15916 when support for non-antialiased rounded rect
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersBottom);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_bottom.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersTop);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_top.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornersRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerTopLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_topleft.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerTopRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_topright.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerBottomLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_bottomleft.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_fill_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1, GCornerBottomRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_origin_rmax_aa_bottomright.${BIT_DEPTH_NAME}.pbi"));
#endif
}

extern uint16_t prv_clamp_corner_radius(GSize size, GCornerMask corner_mask,
                                        uint16_t radius);

void test_graphics_fill_rect__corner_radius(void) {
  // Test 0 radius cases
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(0, 0), GCornerNone, 0), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(0, 0), GCornerNone, 2), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(0, 0), GCornersAll, 0), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(0, 0), GCornersAll, 8), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(0, 5), GCornersAll, 8), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(4, 0), GCornersAll, 8), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(4, 8), GCornerNone, 8), 0);

  // Test minimum of width and height is taken as radius
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(4, 8), GCornersAll, 4), 2);


  cl_assert_equal_i(prv_clamp_corner_radius(GSize(1, 10), GCornersAll, 4), 0);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(2, 10), GCornersAll, 4), 1);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(3, 10), GCornersAll, 4), 1);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(4, 10), GCornersAll, 4), 2);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(5, 10), GCornersAll, 4), 2);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(6, 10), GCornersAll, 4), 3);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(7, 10), GCornersAll, 4), 3);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(8, 10), GCornersAll, 4), 4);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(9, 10), GCornersAll, 4), 4);
  cl_assert_equal_i(prv_clamp_corner_radius(GSize(10, 10), GCornersAll, 4), 4);
}

#define BOX_SIZE 10
#define CLIP_RECT_DRAW_BOX GRect(10, 10, 140, 30)
#define CLIP_RECT_CLIP_BOX GRect(10, 10, 120, 2*BOX_SIZE + 4)
#define CLIP_RECT_RECT_BOX GRect(0, 0, BOX_SIZE, BOX_SIZE)
#define CLIP_OFFSET 40
// yoffset if used to allow for multiple test cases to be drawn and tested on the same test image
// nudge is used to move the rectangle just outside the clipping box on each corner
static void prv_fill_rects(GContext *ctx, uint16_t yoffset, int16_t nudge, int16_t radius) {
  // Adjust drawing box and clipping box
  ctx->draw_state.drawing_box = CLIP_RECT_DRAW_BOX;
  ctx->draw_state.drawing_box.origin.y += yoffset;
  ctx->draw_state.clip_box = CLIP_RECT_CLIP_BOX;
  ctx->draw_state.clip_box.origin.y += yoffset;

  // Top left corner of clipping box
  GRect box = CLIP_RECT_RECT_BOX;
  box.origin.x -= nudge;
  box.origin.y -= nudge;
  graphics_fill_round_rect(ctx, &box, radius, GCornersAll);

  // Top right corner of clipping box
  box.origin.x = CLIP_RECT_CLIP_BOX.size.w - BOX_SIZE;
  box.origin.x += nudge;
  graphics_fill_round_rect(ctx, &box, radius, GCornersAll);

  // Bottom right corner of clipping box
  box.origin.y = CLIP_RECT_CLIP_BOX.size.h - BOX_SIZE;
  box.origin.y += nudge;
  graphics_fill_round_rect(ctx, &box, radius, GCornersAll);

  // Bottom left corner of clipping box
  box.origin.x = 0;
  box.origin.x -= nudge;
  graphics_fill_round_rect(ctx, &box, radius, GCornersAll);
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__clipping_rect(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Fills rectangles around boundaries of clipping box - AA false
  prv_fill_rects(&ctx, 0, 0, 0);
  prv_fill_rects(&ctx, CLIP_OFFSET, 1, 0);
  prv_fill_rects(&ctx, 2 * CLIP_OFFSET, 0, 4);
  prv_fill_rects(&ctx, 3 * CLIP_OFFSET, 1, 4);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_clip_rect.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_fill_rect_${BIT_DEPTH_NAME}__clipping_rect_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, true, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Fills rectangles around boundaries of clipping box - AA true
  prv_fill_rects(&ctx, 0, 0, 0);
  prv_fill_rects(&ctx, CLIP_OFFSET, 1, 0);
  prv_fill_rects(&ctx, 2 * CLIP_OFFSET, 0, 4);
  prv_fill_rects(&ctx, 3 * CLIP_OFFSET, 1, 4);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "fill_rect_clip_rect_aa.${BIT_DEPTH_NAME}.pbi"));
}

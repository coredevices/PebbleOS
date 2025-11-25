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
void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(2, 2, 28, 20), 4);
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(2, 2, 28, 20), 4);
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(2, 2, 28, 20), 4);
}

void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(16, 2, 28, 20), 4);
}

void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(-14, 2, 28, 20), 4);
}

void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(2, 12, 28, 20), 4);
}

void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_round_rect(ctx, &GRect(2, -10, 28, 20), 4);
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 32, 24));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_y_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_y_origin_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 15, 32, 24));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_x_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_nx_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_y_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_across_ny_offset_layer.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__clear(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 32, 24));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_clear.8bit.pbi"));
#else
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
#endif
}

#define RADIUS_DEFAULT             4
#define RECT_WIDTH                 30
#define RECT_HEIGHT                40
#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 20, 20)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 144, 168)
#define ORIGIN_DRAW_RECT_NO_CLIP   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define ORIGIN_DRAW_RECT_CLIP_XY   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define ORIGIN_DRAW_RECT_CLIP_NXNY GRect(-16, -16, RECT_WIDTH, RECT_HEIGHT)

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__origin_aa_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // AA = true, SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 2
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif
  // AA = true, SW = 3
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // AA = true, SW = 4
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // AA = true, SW = 5
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_aa_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__origin_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 2
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 2);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 3
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 3);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // SW = 4
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 4);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // SW = 5
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 5);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r4_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__origin_radius_aa_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // AA = true, SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r0_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r1_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r2_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r3_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax1_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax2_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r0_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r1_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r2_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r3_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax1_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax2_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__origin_radius_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // AA = false, SW = 1
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r0_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r1_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r2_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r3_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax1_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax2_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // AA = false, SW = 11
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 0);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r0_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r1_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 2);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r2_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, 3);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_r3_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) - 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2));
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax1_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Should not draw anything
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &ORIGIN_DRAW_RECT_NO_CLIP, ((MIN(RECT_WIDTH, RECT_HEIGHT)) / 2) + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_origin_rmax2_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
}

#define OFFSET_RECT_NO_CLIP        GRect(20, 10, 144, 168)
#define OFFSET_RECT_CLIP_XY        GRect(20, 10, 20, 20)
#define OFFSET_RECT_CLIP_NXNY      GRect(20, 10, 144, 168)
#define OFFSET_DRAW_RECT_NO_CLIP   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define OFFSET_DRAW_RECT_CLIP_XY   GRect(6, 6, RECT_WIDTH, RECT_HEIGHT)
#define OFFSET_DRAW_RECT_CLIP_NXNY GRect(-16, -16, RECT_WIDTH, RECT_HEIGHT)

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__offset_aa_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // AA = true, SW = 1
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 2
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // AA = true, SW = 3
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // AA = true, SW = 4
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // AA = true, SW = 5
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // AA = true, SW = 11
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));
#endif

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_aa_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_round_rect_${BIT_DEPTH_NAME}__offset_sw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // SW = 1
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 1);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 2
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw2_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 2);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw2_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // SW = 3
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw3_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 3);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw3_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // TODO: Fix offset calculation and reenable this: - PBL-16509
#if SCREEN_COLOR_DEPTH_BITS == 8
  // SW = 4
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw4_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw4_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 4);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw4_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
#endif

  // SW = 5
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw5_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw5_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 5);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw5_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // SW = 11
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, false, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_NO_CLIP, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw11_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, false, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_XY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw11_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, false, 11);
  graphics_draw_round_rect(&ctx, &OFFSET_DRAW_RECT_CLIP_NXNY, RADIUS_DEFAULT);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_round_rect_offset_r4_sw11_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
}


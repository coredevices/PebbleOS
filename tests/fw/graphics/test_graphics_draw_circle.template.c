/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_circle_private.h"
#include "applib/graphics/framebuffer.h"
#include "util/trig.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"

#include "util/size.h"

#include "clar.h"
#include "util.h"
#include "pebble_asserts.h"

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
void test_graphics_draw_circle_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_circle_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(14, 14), 12);
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(14, 14), 12);
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(14, 14), 12);
}

void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(28, 14), 12);
}

void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(-14, 14), 12);
}

void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(14, 28), 12);
}

void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_circle(ctx, GPoint(14, -14), 12);
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 28, 28));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_x_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_nx_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_y_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_ny_origin_layer.${BIT_DEPTH_NAME}.pbi"));
}

#define RADIUS_BIG 15
#define RADIUS_MEDIUM 8
#define RADIUS_MIN_CALCULATED 3
#define RADIUS_MAX_PRECOMPUTED 2
#define RADIUS_SMALL 1
#define RADIUS_NONE 0

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
#define ORIGIN_RECT_CLIP_XY        GRect(0, 0, 30, 40)
#define ORIGIN_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define CENTER_OF_ORIGIN_RECT      GPoint(20, 25)
#define CENTER_OF_ORIGIN_RECT_NXNY GPoint(10, 15)

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__origin_layer_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Big circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r16_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r16_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r16_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Medium circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r8_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r8_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r8_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Small circles
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_XY, ORIGIN_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_NXNY, ORIGIN_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Testing of the special cases for radius:

  // Radius of 3 - starting point for calculated edges
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MIN_CALCULATED);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r3_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Radius of 2 - ending point for precomputed edges
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_MAX_PRECOMPUTED);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r2_no_clip.${BIT_DEPTH_NAME}.pbi"));

  // Radius of 0 - draw a single pixel
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_ORIGIN_RECT_NXNY, RADIUS_NONE);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_origin_aa_r0_no_clip.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 15, 28, 28));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_x_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_nx_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_y_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_across_ny_offset_layer.${BIT_DEPTH_NAME}.pbi"));
}

#define OFFSET_RECT_NO_CLIP        GRect(10, 10, 40, 50)
#define OFFSET_RECT_CLIP_XY        GRect(10, 10, 30, 40)
#define OFFSET_RECT_CLIP_NXNY      GRect(0, 0, 30, 40)
#define CENTER_OF_OFFSET_RECT      GPoint(10, 15)
#define CENTER_OF_OFFSET_RECT_NXNY GPoint(0, 5)

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__offset_layer_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Big circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r16_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r16_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_BIG);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r16_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Medium circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r8_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r8_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_MEDIUM);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r8_clip_nxny.${BIT_DEPTH_NAME}.pbi"));

  // Small circles
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_NO_CLIP, OFFSET_RECT_NO_CLIP, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r1_no_clip.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_XY, OFFSET_RECT_CLIP_XY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r1_clip_xy.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_NXNY, OFFSET_RECT_CLIP_NXNY, true, 1);
  graphics_draw_circle(&ctx, CENTER_OF_OFFSET_RECT_NXNY, RADIUS_SMALL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_aa_r1_clip_nxny.${BIT_DEPTH_NAME}.pbi"));
}

extern void graphics_circle_quadrant_draw_1px_non_aa(GContext* ctx, GPoint p,
                                                     uint16_t radius,
                                                     GCornerMask quadrant);

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__quadrants(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornerTopLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quad_top_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornerTopRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quad_top_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornerBottomLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quad_bottom_left.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornerBottomRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quad_bottom_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornersTop);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quads_top.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornersBottom);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quads_bottom.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornersRight);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quads_right.${BIT_DEPTH_NAME}.pbi"));

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_circle_quadrant_draw_1px_non_aa(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_MEDIUM, GCornersLeft);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_offset_r8_quads_left.${BIT_DEPTH_NAME}.pbi"));
}

extern void graphics_circle_quadrant_draw_1px_aa(GContext* ctx, GPoint p,
                                                 uint16_t radius,
                                                 GCornerMask quadrant);

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__quadrants_aa(void) {
  #if PBL_COLOR
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  typedef struct {
    char *filename_part;
    GCornerMask mask;
  } TestConfig;

  TestConfig test_config[] = {
      {
          .filename_part = "quad_top_left",
          .mask = GCornerTopLeft,
      },
      {
          .filename_part = "quad_top_right",
          .mask = GCornerTopRight,
      },
      {
          .filename_part = "quad_bottom_right",
          .mask = GCornerBottomRight,
      },
      {
          .filename_part = "quad_bottom_left",
          .mask = GCornerBottomLeft,
      },
      {
          .filename_part = "quads_top",
          .mask = GCornersTop,
      },
      {
          .filename_part = "quads_bottom",
          .mask = GCornersBottom,
      },
      {
          .filename_part = "quads_right",
          .mask = GCornersRight,
      },
      {
          .filename_part = "quads_left",
          .mask = GCornersLeft,
      },
  };

  // note: not the prettiest, fast a quick way to render all the scenarios Nitin was interested in
  for (int i = 0; i < ARRAY_LENGTH(test_config); i++) {
    TestConfig c = test_config[i];
    setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
    GPoint pt = CENTER_OF_ORIGIN_RECT;
    // draw multiple quads with different radiuses
    for (int r = 0; r <= 15; r++) {
      graphics_circle_quadrant_draw_1px_aa(&ctx, pt, r, c.mask);

      // center point follows a grid
      pt.x += 30;
      if (pt.x > 120) {
        pt.x = CENTER_OF_ORIGIN_RECT.x;
        pt.y += 30;
      }
    }

    // construct file name and create meaningful assert description
    char filename[100];
    snprintf(filename, sizeof(filename),
        "draw_circle_offset_aa_r8_%s.${BIT_DEPTH_NAME}.pbi", c.filename_part);
    cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, filename));
  }
  #endif
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__clear(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 60, 60));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
#if PBL_COLOR
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_clear.8bit.pbi"));
#else
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
#endif
}

// Draws circles in a grid pattern while increasing the stroke width for each circle
static void prv_draw_circles(GContext *ctx) {
  GPoint pt = CENTER_OF_ORIGIN_RECT;
  for (int sw = 0; sw <= 10; sw++) {
    ctx->draw_state.stroke_width = sw;
    graphics_draw_circle(ctx, pt, 0);

    // center point follows a grid
    pt.x += 30;
    if (pt.x > 120) {
      pt.x = CENTER_OF_ORIGIN_RECT.x;
      pt.y += 30;
    }
  }
}

#define TO_TRIG(deg) (((deg) * TRIG_MAX_ANGLE) / 360)

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__zero_swX_black(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw black circles with different alpha values in the stroke color with antialiasing disabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 0);
  graphics_context_set_stroke_color(&ctx, GColorBlack);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_swX_black.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__zero_aa_swX_black(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw black circles with different alpha values in the stroke color with antialiasing enabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 0);
  graphics_context_set_stroke_color(&ctx, GColorBlack);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_aa_swX_black.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_8bit__zero_swX_color(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw red circles with different alpha values in the stroke color with antialiasing disabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 0);
  graphics_context_set_stroke_color(&ctx, GColorRed);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_swX_color.8bit.pbi"));
}

void test_graphics_draw_circle_8bit__zero_aa_swX_color(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw red circles with different alpha values in the stroke color with antialiasing enabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 0);
  graphics_context_set_stroke_color(&ctx, GColorRed);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_aa_swX_color.8bit.pbi"));
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__zero_swX_clear(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw clear circles with antialiasing disabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 0);
  graphics_context_set_stroke_color(&ctx, GColorClear);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_swX_clear.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__zero_aa_swX_clear(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw clear circles with antialiasing enabled

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 0);
  graphics_context_set_stroke_color(&ctx, GColorClear);
  prv_draw_circles(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_circle_r0_aa_swX_clear.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__arc_stroked(void){
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Letter C
  uint32_t angle_end = TRIG_MAX_ANGLE + (TRIG_MAX_ANGLE / 8);
  uint32_t angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_letter_c.${BIT_DEPTH_NAME}.pbi"));

  // Stroke width bigger than radius
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, RADIUS_BIG * 2);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_stroke_bigger_than_radius.${BIT_DEPTH_NAME}.pbi"));

  // Negative angle letter C
  angle_end -= TRIG_MAX_ANGLE;
  angle_start -= TRIG_MAX_ANGLE;
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_letter_c.${BIT_DEPTH_NAME}.pbi"));

  // More negative angle letter C (both angle are actually negative)
  angle_end -= TRIG_MAX_ANGLE;
  angle_start -= TRIG_MAX_ANGLE;
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_letter_c.${BIT_DEPTH_NAME}.pbi"));

  // Incorrect angles (angle_start > angle_end) - should result in empty image
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, RADIUS_BIG * 2);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_end, angle_start);
  cl_check(framebuffer_is_empty(NULL, ctx.parent_framebuffer, GColorWhite));

  // table with most popular angles to test
  typedef struct {
    char *filename_part;
    uint32_t angle;
  } TestAngles;

  TestAngles test_angles[] = {
    {
      .filename_part = "__1_degrees",
      .angle = TRIG_MAX_ANGLE / 360,
    },
    {
      .filename_part = "__6_degrees",
      .angle = TRIG_MAX_ANGLE / 60,
    },
    {
      .filename_part = "_30_degrees",
      .angle = TRIG_MAX_ANGLE / 12,
    },
    {
      .filename_part = "_45_degrees",
      .angle = TRIG_MAX_ANGLE / 8,
    },
    {
      .filename_part = "_90_degrees",
      .angle = TRIG_MAX_ANGLE / 4,
    },
    {
      .filename_part = "181_degrees",
      .angle = TRIG_MAX_ANGLE / 2 + TRIG_MAX_ANGLE / 360,
    }
  };

  for (int i = 0; i < ARRAY_LENGTH(test_angles); i++) {
    setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
    GPoint pt = CENTER_OF_ORIGIN_RECT;
    uint16_t radius = 10;
    uint16_t stroke_width = 1;

    for (int r = 1; r <= 7; r++) {
      graphics_context_set_stroke_width(&ctx, stroke_width);
      graphics_draw_arc_internal(&ctx, pt, radius, 0, test_angles[i].angle);

      stroke_width += 1;
      radius += 3;
      pt.x += (radius + stroke_width) * 2;
      if (pt.x > 120) {
        pt.x = CENTER_OF_ORIGIN_RECT.x;
        pt.y += (radius + stroke_width) * 2;
      }
    }

    char filename[100];
    snprintf(filename, sizeof(filename),
             "draw_arc_offset_aa_end_angle_%s.${BIT_DEPTH_NAME}.pbi", test_angles[i].filename_part);
    cl_check_(gbitmap_pbi_eq(&ctx.dest_bitmap, filename), filename);
  }

  for (int i = 0; i < ARRAY_LENGTH(test_angles); i++) {
    setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
    GPoint pt = CENTER_OF_ORIGIN_RECT;
    uint16_t radius = 10;
    uint16_t stroke_width = 1;

    for (int r = 1; r <= 7; r++) {
      graphics_context_set_stroke_width(&ctx, stroke_width);
      graphics_draw_arc_internal(&ctx, pt, radius, test_angles[i].angle, TRIG_MAX_ANGLE);

      stroke_width += 1;
      radius += 3;
      pt.x += (radius + stroke_width) * 2;
      if (pt.x > 120) {
        pt.x = CENTER_OF_ORIGIN_RECT.x;
        pt.y += (radius + stroke_width) * 2;
      }
    }

    char filename[100];
    snprintf(filename, sizeof(filename),
             "draw_arc_offset_aa_start_angle_%s.${BIT_DEPTH_NAME}.pbi", test_angles[i].filename_part);
    cl_check_(gbitmap_pbi_eq(&ctx.dest_bitmap, filename), filename);
  }

  // Additional 90° end_angle unit tests:
  angle_start = TO_TRIG(45);
  angle_end = TO_TRIG(90);
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 5);
  GPoint pt = CENTER_OF_ORIGIN_RECT;

  // This test is sponsored by number 4
  for (int r = 0; r < 4; r++) {
    graphics_draw_arc_internal(&ctx, pt, RADIUS_BIG, angle_start, angle_end);

    angle_start += TRIG_MAX_ANGLE/4;
    angle_end += TRIG_MAX_ANGLE/4;
    pt.x += RADIUS_BIG * 4;
    if (pt.x > 120) {
      pt.x = CENTER_OF_ORIGIN_RECT.x;
      pt.y += RADIUS_BIG * 4;
    }
  }

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_aa_end_angle_on_divider.${BIT_DEPTH_NAME}.pbi"));
}

void test_graphics_draw_circle_8bit__arc_colors(void){
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Letter C
  uint32_t angle_end = TRIG_MAX_ANGLE + (TRIG_MAX_ANGLE / 8);
  uint32_t angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 1);
  graphics_context_set_fill_color(&ctx, GColorRed);
  graphics_context_set_stroke_color(&ctx, GColorGreen);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_letter_c_color_1px.8bit.pbi"));

  // Letter C SW 4
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);
  graphics_context_set_fill_color(&ctx, GColorRed);
  graphics_context_set_stroke_color(&ctx, GColorGreen);
  graphics_draw_arc_internal(&ctx, CENTER_OF_ORIGIN_RECT, RADIUS_BIG, angle_start, angle_end);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_arc_origin_aa_letter_c_color_4px.8bit.pbi"));
}

#define DRAWING_SPACING 40
char *mode_names[] = {
  "_90_degrees",
  "180_degrees",
  "270_degrees",
};

char *precision_modes[] = {
  "without_faction",
  "with_fraction",
};

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__radial_precise(void){
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  uint32_t angle_start = TRIG_MAX_ANGLE / 8;
  uint32_t angle_end = angle_start + (TRIG_MAX_ANGLE / 4);
  GPointPrecise center = GPointPrecise(CENTER_OF_ORIGIN_RECT.x * 8, CENTER_OF_ORIGIN_RECT.y * 8);
  Fixed_S16_3 radius = (Fixed_S16_3){.integer = RADIUS_BIG};

  for (int precision_mode = 0; precision_mode < 2; precision_mode++) {
    for (int mode = 0; mode < 3; mode++) {
      setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, true, 4);

      for (int angle_offset=0; angle_offset < 4; angle_offset++) {
        graphics_draw_arc_precise_internal(&ctx, center, radius, angle_start, angle_end);

        center.x.integer += (angle_offset % 2 == 0) ? DRAWING_SPACING : -DRAWING_SPACING;
        center.y.integer += (angle_offset == 1) ? DRAWING_SPACING : 0;

        angle_start += TRIG_MAX_ANGLE / 4;
        angle_end += TRIG_MAX_ANGLE / 4;
      }

      char filename[100];
      snprintf(filename, sizeof(filename),
               "draw_arc_origin_aa_precise_%s_%s_.${BIT_DEPTH_NAME}.pbi", precision_modes[precision_mode], mode_names[mode]);
      cl_check_(gbitmap_pbi_eq(&ctx.dest_bitmap, filename), filename);

      center.y.integer -= DRAWING_SPACING;
      angle_end += TRIG_MAX_ANGLE / 4;
    }

    center.x.fraction = 4;
    center.y.fraction = 4;
    radius.fraction = 4;
    angle_start = TRIG_MAX_ANGLE / 8;
    angle_end = angle_start + (TRIG_MAX_ANGLE / 4);
  }
}

void test_graphics_draw_circle_${BIT_DEPTH_NAME}__calc_draw_config_caps(void) {
  // ## Section with special cases
  // Test for no angles
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0), TO_TRIG(0)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone}
  }));

  // Tests for 360° angle
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0), TO_TRIG(360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone},
    .full_quadrants = GCornersAll,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(20), TO_TRIG(380)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone},
    .full_quadrants = GCornersAll,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone}
  }));

  // Tests for overflowing angles
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(20), TO_TRIG(1000)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone},
    .full_quadrants = GCornersAll,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerNone}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(360), TO_TRIG(370)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(10),GCornerTopRight}
  }));

  // # Section with quadrant filling
  // Tests for full quadrants
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0), TO_TRIG(90)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight},
    .full_quadrants = GCornerTopRight,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(90), TO_TRIG(180)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight},
    .full_quadrants = GCornerBottomRight,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(180), TO_TRIG(270)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft},
    .full_quadrants = GCornerBottomLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(270), TO_TRIG(360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft},
    .full_quadrants = GCornerTopLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight}
  }));

  // Tests for pairs of quadrants
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0), TO_TRIG(180)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight},
    .full_quadrants = GCornersRight,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(180), TO_TRIG(360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft},
    .full_quadrants = GCornersLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(270), TO_TRIG(360 + 90)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft},
    .full_quadrants = GCornersTop,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(90), TO_TRIG(270)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight},
    .full_quadrants = GCornersBottom,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft}
  }));

  // Tests for triples of quadrants
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0), TO_TRIG(270)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight},
    .full_quadrants = (GCornersBottom | GCornerTopRight),
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(90), TO_TRIG(360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight},
    .full_quadrants = (GCornersBottom | GCornerTopLeft),
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(0),GCornerTopRight}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(180), TO_TRIG(360 + 90)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft},
    .full_quadrants = (GCornersTop | GCornerBottomLeft),
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight}
  }));

  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(270), TO_TRIG(360 + 180)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270),GCornerTopLeft},
    .full_quadrants = (GCornersTop | GCornerBottomRight),
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft}
  }));

  // ## Section with regular angles
  // Within same quadrant
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(30), TO_TRIG(60)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(30),GCornerTopRight},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(60),GCornerTopRight}
  }));

  // Starting quadrant filling up and ending quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(45), TO_TRIG(270 + 45)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight},
    .full_quadrants = GCornersBottom,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270 + 45),GCornerTopLeft}
  }));

  // Ending quadrant filling up and starting quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(90 + 45), TO_TRIG(360 + 45)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90 + 45),GCornerBottomRight},
    .full_quadrants = GCornersLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight}
  }));

  // ## Section with regular angles but over 360°
  // Within same quadrant
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(360 + 30), TO_TRIG(360 + 60)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(30),GCornerTopRight},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(60),GCornerTopRight}
  }));

  // Starting quadrant filling up and ending quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(360 + 45), TO_TRIG(360 + 270 + 45)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight},
    .full_quadrants = GCornersBottom,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270 + 45),GCornerTopLeft}
  }));

  // Ending quadrant filling up and starting quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(360 + 90 + 45), TO_TRIG(360 + 360 + 45)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90 + 45),GCornerBottomRight},
    .full_quadrants = GCornersLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight}
  }));

  // ## Section with negative angles
  // Within same quadrant
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(30 - 360), TO_TRIG(60 - 360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(30)+1,GCornerTopRight},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(60)+1,GCornerTopRight}
  }));

  // Starting quadrant filling up and ending quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(45 - 360), TO_TRIG(270 + 45 - 360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight},
    .full_quadrants = GCornersBottom,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270 + 45),GCornerTopLeft}
  }));

  // Ending quadrant filling up and starting quadrant finishing
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(90 + 45 - 360), TO_TRIG(360 + 45 - 360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90 + 45),GCornerBottomRight},
    .full_quadrants = GCornersLeft,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight}
  }));

  // Pair of quadrants
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(0 - 360), TO_TRIG(180 - 360)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(360),GCornerTopRight},
    .full_quadrants = GCornersRight,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(180),GCornerBottomLeft}
  }));

  // Negative to positive
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(-45), TO_TRIG(45)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(270 + 45),GCornerTopLeft},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(45),GCornerTopRight}
  }));

  // Flipping quadrant issue
  cl_assert_equal_edc(prv_calc_draw_config_ellipsis(TO_TRIG(70), TO_TRIG(90)), ((EllipsisDrawConfig){
    .start_quadrant = (EllipsisPartDrawConfig){TO_TRIG(70),GCornerTopRight},
    .full_quadrants = GCornerNone,
    .end_quadrant = (EllipsisPartDrawConfig){TO_TRIG(90),GCornerBottomRight}
  }));
}

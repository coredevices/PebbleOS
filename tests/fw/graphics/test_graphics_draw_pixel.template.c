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
void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(5, 5));
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_pixel(ctx, GPoint(5, 5));
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_draw_pixel(ctx, GPoint(5, 5));
}

void outside_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(15, 5));
}

void outside_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(-5, 5));
}

void outside_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(5, 15));
}

void outside_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(5, -5));
}

void outside_x_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(15, 15));
}

void outside_nx_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(-5, 15));
}

void outside_x_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(15, -5));
}

void outside_nx_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, GPoint(-5, -5));
}

void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 10, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_x_origin_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_y_origin_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_x_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_x_y_origin_layer", ctx.parent_framebuffer, GColorWhite));
}

void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 10, 10, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_inside_offset_layer.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_x_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_nx_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_y_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_ny_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_x_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_x_y_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_nx_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_nx_y_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_x_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_x_ny_offset_layer", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &outside_nx_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("outside_nx_ny_offset_layer", ctx.parent_framebuffer, GColorWhite));
}

void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__clear(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 10, 10));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_inside_origin_layer.${BIT_DEPTH_NAME}.pbi"));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_clear.8bit.pbi"));
#else
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
#endif
}

#define BOX_OFFSET_X 8
#define BOX_OFFSET_Y 4
#define COLUMN_OFFSET_X 32
// Draws an 8x4 rectangle starting a point p
static void prv_draw_box(GContext *ctx, GPoint p) {
  graphics_draw_pixel(ctx, GPoint(p.x + 0, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 1, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 2, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 3, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 4, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 5, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 6, p.y));
  graphics_draw_pixel(ctx, GPoint(p.x + 7, p.y));

  graphics_draw_pixel(ctx, GPoint(p.x + 0, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 1, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 2, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 3, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 4, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 5, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 6, p.y + 1));
  graphics_draw_pixel(ctx, GPoint(p.x + 7, p.y + 1));

  graphics_draw_pixel(ctx, GPoint(p.x + 0, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 1, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 2, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 3, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 4, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 5, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 6, p.y + 2));
  graphics_draw_pixel(ctx, GPoint(p.x + 7, p.y + 2));

  graphics_draw_pixel(ctx, GPoint(p.x + 0, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 1, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 2, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 3, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 4, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 5, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 6, p.y + 3));
  graphics_draw_pixel(ctx, GPoint(p.x + 7, p.y + 3));
}

// Draws two columns of colors (first 32 colors in first column, second 32 colors in second column)
// Offsets the two color columns based on input transparency
static void prv_draw_boxes(GContext *ctx, uint8_t transparency) {
  int column = 3 - transparency;
  for (uint8_t color_index = 0; color_index < 64; color_index++) {
    uint8_t color_col = (color_index < 32) ? 0 : 1;
    ctx->draw_state.stroke_color = (GColor) { .argb = ((transparency << 6) | color_index) };
    prv_draw_box(ctx, GPoint((4 + (BOX_OFFSET_X * color_col) + (COLUMN_OFFSET_X * column)),
                             (BOX_OFFSET_Y + ((color_index - (32 * color_col)) * 4 ))));
  }
}

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, 144, 168)
void test_graphics_draw_pixel_8bit__transparent(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_context_set_fill_color(&ctx, GColorBlack);
  graphics_fill_rect(&ctx, &ORIGIN_RECT_NO_CLIP);

  // No transparency
  prv_draw_boxes(&ctx, 3);

  // 33% transparency
  prv_draw_boxes(&ctx, 2);

  // 66% transparency - should draw nothing according to current implementation
  prv_draw_boxes(&ctx, 1);

  // 100% transparency - should draw nothing according to current implementation
  prv_draw_boxes(&ctx, 0);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_transparent.8bit.pbi"));
}


#define CLIP_RECT_DRAW_BOX GRect(10, 10, 40, 40)
#define CLIP_RECT_CLIP_BOX GRect(10, 10, 20, 20)
static void prv_draw_pixels(GContext *ctx, bool aa, uint8_t sw) {
  test_graphics_context_reset(ctx, fb);
  setup_test_aa_sw(ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, aa, sw);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  // Left boundary
  graphics_draw_pixel(ctx, GPoint(-1, 5));
  graphics_draw_pixel(ctx, GPoint(0, 10));
  graphics_draw_pixel(ctx, GPoint(1, 15));

  // Right boundary
  graphics_draw_pixel(ctx, GPoint(19, 5));
  graphics_draw_pixel(ctx, GPoint(20, 10));
  graphics_draw_pixel(ctx, GPoint(21, 15));

  // Top boundary
  graphics_draw_pixel(ctx, GPoint(5, -1));
  graphics_draw_pixel(ctx, GPoint(10, 0));
  graphics_draw_pixel(ctx, GPoint(15, 1));

  // Bottom boundary
  graphics_draw_pixel(ctx, GPoint(5, 19));
  graphics_draw_pixel(ctx, GPoint(10, 20));
  graphics_draw_pixel(ctx, GPoint(15, 21));
}

void test_graphics_draw_pixel_${BIT_DEPTH_NAME}__clipping_rect(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Draw pixels around boundaries of clipping box - AA false, SW 1
  prv_draw_pixels(&ctx, false, 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_pixel_clip_rect.${BIT_DEPTH_NAME}.pbi"));
}

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

#if SCREEN_COLOR_DEPTH_BITS == 8
  #include "8bit/test_framebuffer.h"
#else
  #include "1bit/test_framebuffer.h"
#endif

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_draw_line__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_line__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void inside_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void white_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void clear_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorClear);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, 35), GPoint(45, 40));
}

void across_x_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(50, 40), GPoint(70, 35));
}

void across_nx_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(-25, 35), GPoint(15, 40));
}

void across_y_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(40, 50), GPoint(35, 70));
}

void across_ny_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(5, -30), GPoint(45, 30));
}

void test_graphics_draw_line__origin_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 60, 60));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_inside_origin_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_x_origin_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_nx_origin_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_y_origin_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_ny_origin_layer")));
}

void test_graphics_draw_line__offset_layer(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(10, 10, 60, 60));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_inside_offset_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_x_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_x_offset_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_nx_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_nx_offset_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_y_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_y_offset_layer")));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &across_ny_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_across_ny_offset_layer")));
}

void test_graphics_draw_line__clear(void) {
  GContext ctx;
  Layer layer;
  test_graphics_context_init(&ctx, fb);
  layer_init(&layer, &GRect(0, 0, 60, 60));
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_inside_origin_layer")));
  layer_set_update_proc(&layer, &white_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(framebuffer_is_empty("white_over_black", ctx.parent_framebuffer, GColorWhite));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &inside_layer_update_callback);
  layer_render_tree(&layer, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_line_inside_origin_layer")));
  layer_set_update_proc(&layer, &clear_layer_update_callback);
  layer_render_tree(&layer, &ctx);
#if SCREEN_COLOR_DEPTH_BITS == 8
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_line_clear")));
#else
  cl_check(framebuffer_is_empty("clear_over_black", ctx.parent_framebuffer, GColorWhite));
#endif
}

#define MAX_NUM_ROWS 168
#define MAX_NUM_COLS 144

#define ORIGIN_RECT_NO_CLIP        GRect(0, 0, MAX_NUM_COLS, MAX_NUM_ROWS)
#define ORIGIN_RECT_CLIP_EVEN      GRect(10, 10, 60, 60)
#define ORIGIN_RECT_CLIP_ODD       GRect(11, 11, 61, 61)
void test_graphics_draw_line___origin_horizontal_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even rows draw appropriately
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 12), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 23), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 13), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 24), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_no_clip")));

  // Even rows of different lengths
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 2), 148);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 4), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 6), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 8), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 10), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 12), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 14), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 16), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 18), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 20), 143);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 22), 145);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_even_rows_no_clip")));

  // Odd rows of different lengths
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 1), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 3), 148);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 5), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 7), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 9), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 11), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 13), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 15), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 17), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 19), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 21), 143);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 23), 145);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_odd_rows_no_clip")));

  // Test to make sure drawing on all rows creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_EVEN, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_ODD, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_origin_odd_clip")));
}

#define OFFSET_RECT_EVEN           GRect(14, 14, MAX_NUM_COLS, MAX_NUM_ROWS)
#define OFFSET_RECT_ODD            GRect(15, 15, MAX_NUM_COLS, MAX_NUM_ROWS)
#define OFFSET_RECT_CLIP_EVEN      GRect(10, 10, 60, 60)
#define OFFSET_RECT_CLIP_ODD       GRect(11, 11, 61, 61)
void test_graphics_draw_line___even_offset_horizontal_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even rows draw appropriately
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 12), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 23), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 13), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 24), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_no_clip")));

  // Even rows of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 2), MAX_NUM_COLS + 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 4), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 6), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 8), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 10), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 12), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 14), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 16), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 18), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 20), MAX_NUM_COLS - 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 22), MAX_NUM_COLS + 1);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_even_rows_no_clip")));

  // Odd rows of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 1), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 3), MAX_NUM_COLS + 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 5), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 7), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 9), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 11), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 13), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 15), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 17), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 19), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 21), MAX_NUM_COLS - 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 23), MAX_NUM_COLS + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_odd_rows_no_clip")));

  // Test to make sure drawing on all rows creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_EVEN, OFFSET_RECT_EVEN, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_ODD, OFFSET_RECT_EVEN, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_even_offset_odd_clip")));
}

void test_graphics_draw_line___odd_offset_horizontal_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even rows draw appropriately
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 12), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(6, 23), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 13), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(7, 24), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_no_clip")));

  // Even rows of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 2), MAX_NUM_COLS + 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 4), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 6), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 8), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 10), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 12), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 14), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 16), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 18), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 20), MAX_NUM_COLS - 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 22), MAX_NUM_COLS + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_even_rows_no_clip")));

  // Odd rows of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 1), MAX_NUM_COLS);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 3), MAX_NUM_COLS + 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 5), 0);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 7), 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 9), 2);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 11), 3);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 13), 4);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 15), 20);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 17), 21);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 19), 22);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 21), MAX_NUM_COLS - 1);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, 23), MAX_NUM_COLS + 1);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_odd_rows_no_clip")));

  // Test to make sure drawing on all rows creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_EVEN, OFFSET_RECT_ODD, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_ODD, OFFSET_RECT_ODD, false, 1);
  for (int16_t row = 0; row < MAX_NUM_ROWS; row++) {
    graphics_draw_horizontal_line_dotted(&ctx, GPoint(0, row), MAX_NUM_COLS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_horiz_dotted_line_odd_offset_odd_clip")));
}

void test_graphics_draw_line___origin_vertical_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even cols draw appropriately
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 7), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(24, 7), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_no_clip")));

  // Even cols of different lengths
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(2, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(4, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(6, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(8, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(14, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(16, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(18, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(20, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(22, 0), MAX_NUM_ROWS + 1 );

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_even_cols_no_clip")));

  // Odd cols of different lengths
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(1, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(3, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(5, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(7, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(9, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(11, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(15, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(17, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(19, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(21, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 0), MAX_NUM_ROWS + 1 );
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_odd_cols_no_clip")));

  // Test to make sure drawing on all cols creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_EVEN, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_CLIP_ODD, ORIGIN_RECT_NO_CLIP, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_origin_odd_clip")));
}

void test_graphics_draw_line___even_offset_vertical_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even cols draw appropriately
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 7), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(24, 7), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_no_clip")));

  // Even cols of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(2, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(4, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(6, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(8, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(14, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(16, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(18, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(20, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(22, 0), MAX_NUM_ROWS + 1 );

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_even_cols_no_clip")));

  // Odd cols of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(1, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(3, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(5, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(7, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(9, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(11, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(15, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(17, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(19, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(21, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 0), MAX_NUM_ROWS + 1 );
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_odd_cols_no_clip")));

  // Test to make sure drawing on all cols creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_EVEN, OFFSET_RECT_EVEN, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_EVEN, OFFSET_RECT_EVEN, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_ODD, OFFSET_RECT_EVEN, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_even_offset_odd_clip")));
}

void test_graphics_draw_line___odd_offset_vertical_dotted(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test odd and even cols draw appropriately
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 6), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 7), 10);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(24, 7), 10);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_no_clip")));

  // Even cols of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(0, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(2, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(4, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(6, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(8, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(12, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(14, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(16, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(18, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(20, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(22, 0), MAX_NUM_ROWS + 1 );
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_even_cols_no_clip")));

  // Odd cols of different lengths
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(1, 0), MAX_NUM_ROWS);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(3, 0), MAX_NUM_ROWS + 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(5, 0), 0);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(7, 0), 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(9, 0), 2);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(11, 0), 3);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(13, 0), 4);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(15, 0), 20);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(17, 0), 21);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(19, 0), 22);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(21, 0), MAX_NUM_ROWS - 1);
  graphics_draw_vertical_line_dotted(&ctx, GPoint(23, 0), MAX_NUM_ROWS + 1 );
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_odd_cols_no_clip")));

  // Test to make sure drawing on all cols creates checkerboard pattern
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_ODD, OFFSET_RECT_ODD, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_checkerboard_no_clip")));

  // Clipping on even boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_EVEN, OFFSET_RECT_ODD, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_even_clip")));

  // Clipping on odd boundaries - no offset
  setup_test_aa_sw(&ctx, fb, OFFSET_RECT_CLIP_ODD, OFFSET_RECT_ODD, false, 1);
  for (int16_t col = 0; col < MAX_NUM_COLS; col++) {
    graphics_draw_vertical_line_dotted(&ctx, GPoint(col, 0), MAX_NUM_ROWS);
  }
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
			     TEST_NAMED_PBI_FILE("draw_vert_dotted_line_odd_offset_odd_clip")));
}

void test_graphics_draw_line__dotted_cross(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test horizontal and vertical lines cross appropriately
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  // cross - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 10), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(5, 15), 10);

  // cross - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(41, 11), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(36, 16), 10);

  // T facing up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(70, 15), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(65, 15), 10);

  // T facing down
  graphics_draw_vertical_line_dotted(&ctx, GPoint(101, 11), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(96, 16), 10);

  // cross - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 32), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(4, 36), 10);

  // cross - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(41, 33), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(37, 37), 10);

  // T facing up - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(70, 36), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(66, 36), 10);

  // T facing down - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(101, 33), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(97, 37), 10);


  // T facing left - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(5, 76), 5);

  // T facing right - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(50, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(45, 75), 5);

  // T facing right - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(71, 71), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(66, 76), 5);

  // T facing left - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(100, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(101, 75), 5);

  // T facing left - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(131, 71), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(132, 76), 5);


  // T facing right - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(10, 96), 5);

  // T facing right - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(50, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(46, 95), 5);

  // T facing right - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(71, 91), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(67, 96), 5);

  // T facing left - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(100, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(100, 95), 5);

  // T facing left - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(131, 91), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(131, 96), 5);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_dotted_line_cross")));
}

void test_graphics_draw_line_8bit__dotted_cross_color(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test horizontal and vertical lines cross appropriately
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);
  graphics_context_set_stroke_color(&ctx, GColorRed);
  // cross - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 10), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(5, 15), 10);

  // cross - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(41, 11), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(36, 16), 10);

  // T facing up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(70, 15), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(65, 15), 10);

  // T facing down
  graphics_draw_vertical_line_dotted(&ctx, GPoint(101, 11), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(96, 16), 10);

  // cross - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 32), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(4, 36), 10);

  // cross - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(41, 33), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(37, 37), 10);

  // T facing up - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(70, 36), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(66, 36), 10);

  // T facing down - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(101, 33), 5);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(97, 37), 10);


  // T facing left - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(5, 76), 5);

  // T facing right - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(50, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(45, 75), 5);

  // T facing right - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(71, 71), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(66, 76), 5);

  // T facing left - even vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(100, 70), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(101, 75), 5);

  // T facing left - odd vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(131, 71), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(132, 76), 5);


  // T facing right - lined up
  graphics_draw_vertical_line_dotted(&ctx, GPoint(10, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(10, 96), 5);

  // T facing right - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(50, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(46, 95), 5);

  // T facing right - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(71, 91), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(67, 96), 5);

  // T facing left - even vert, even horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(100, 90), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(100, 95), 5);

  // T facing left - odd vert, odd horiz
  graphics_draw_vertical_line_dotted(&ctx, GPoint(131, 91), 10);
  graphics_draw_horizontal_line_dotted(&ctx, GPoint(131, 96), 5);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_dotted_line_cross_color")));
}

static void draw_lines_same_point(GContext *ctx) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(10,10), GPoint(10,10));

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(20,20), GPoint(20,20));

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(30,30), GPoint(30,30));

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(40,40), GPoint(40,40));

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(50,50), GPoint(50,50));

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(60,60), GPoint(60,60));

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, GPoint(70,70), GPoint(70,70));

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, GPoint(80,80), GPoint(80,80));

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, GPoint(90,90), GPoint(90,90));

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, GPoint(100,100), GPoint(100,100));
}

void test_graphics_draw_line__same_point(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, ORIGIN_RECT_NO_CLIP, ORIGIN_RECT_NO_CLIP, false, 1);

  draw_lines_same_point(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_line_same_point")));

#if SCREEN_COLOR_DEPTH_BITS == 8
  graphics_context_set_stroke_color(&ctx, GColorRed);
  draw_lines_same_point(&ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_line_same_point_color")));
#endif
}

#define CLIP_RECT_DRAW_BOX GRect(10, 10, 40, 40)
#define CLIP_RECT_CLIP_BOX GRect(10, 10, 20, 20)
#define CLIP_OFFSET 100
static void prv_draw_lines(GContext *ctx, uint8_t sw, uint16_t xoffset, uint16_t yoffset) {
  // Adjust drawing box and clipping box
  ctx->draw_state.drawing_box = CLIP_RECT_DRAW_BOX;
  ctx->draw_state.drawing_box.origin.x += xoffset;
  ctx->draw_state.drawing_box.origin.y += yoffset;
  ctx->draw_state.clip_box = CLIP_RECT_CLIP_BOX;
  ctx->draw_state.clip_box.origin.x += xoffset;
  ctx->draw_state.clip_box.origin.y += yoffset;
  graphics_context_set_stroke_width(ctx, sw);

  graphics_draw_line(ctx, GPoint(-2, 10), GPoint(2, 10));  // left
  graphics_draw_line(ctx, GPoint(-2, 5), GPoint(5, -2));   // top left corner

  graphics_draw_line(ctx, GPoint(10, -2), GPoint(10, 2));  // top
  graphics_draw_line(ctx, GPoint(15, -2), GPoint(22, 5));  // top right corner

  graphics_draw_line(ctx, GPoint(18, 10), GPoint(22, 10)); // right
  graphics_draw_line(ctx, GPoint(22, 15), GPoint(15, 22)); // bottom right corner

  graphics_draw_line(ctx, GPoint(10, 18), GPoint(10, 22));  // bottom
  graphics_draw_line(ctx, GPoint(5, 22), GPoint(-2, 15));    // bottom left corner
}

void test_graphics_draw_line__clipping_rect(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Draw lines around boundaries of clipping box - AA false, SW 1
  prv_draw_lines(&ctx, 1, 0, 0);

  // Draw lines around boundaries of clipping box - AA false, SW 2
  prv_draw_lines(&ctx, 2, CLIP_OFFSET, 0);

  // Draw lines around boundaries of clipping box - AA false, SW 3
  prv_draw_lines(&ctx, 3, 0, CLIP_OFFSET);

  // Draw lines around boundaries of clipping box - AA false, SW 4
  prv_draw_lines(&ctx, 4, CLIP_OFFSET, CLIP_OFFSET);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_line_clip_rect")));
}

void test_graphics_draw_line__clipping_rect_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, true, 1);
  graphics_context_set_stroke_color(&ctx, GColorBlack);

  // Draw lines around boundaries of clipping box - AA true, SW 1
  prv_draw_lines(&ctx, 1, 0, 0);

  // Draw lines around boundaries of clipping box - AA true, SW 2
  prv_draw_lines(&ctx, 2, CLIP_OFFSET, 0);

  // Draw lines around boundaries of clipping box - AA true, SW 3
  prv_draw_lines(&ctx, 3, 0, CLIP_OFFSET);

  // Draw lines around boundaries of clipping box - AA true, SW 4
  prv_draw_lines(&ctx, 4, CLIP_OFFSET, CLIP_OFFSET);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_line_clip_rect_aa")));
}

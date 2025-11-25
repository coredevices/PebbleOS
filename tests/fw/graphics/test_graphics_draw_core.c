/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_private.h"
#include "applib/graphics/graphics_private_raw.h"
#include "applib/graphics/framebuffer.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"


#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "8bit/test_framebuffer.h"


// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

// Fakes
////////////////////////////////////
#include "fake_gbitmap_get_data_row.h"

static FrameBuffer *fb = NULL;

#define CLIP_RECT_DRAW_BOX GRect(0, 0, DISP_COLS, DISP_ROWS)
#define CLIP_RECT_CLIP_BOX GRect(0, 0, DISP_COLS, DISP_ROWS)

// Setup and Teardown
////////////////////////////////////

// Setup
void test_graphics_draw_core__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) { DISP_COLS, DISP_ROWS });
  // Enable fake data row handling which will override the gbitmap_get_data_row_xxx() functions
  // with their fake counterparts in fake_gbitmap_get_data_row.c
  s_fake_data_row_handling = true;
}

// Teardown
void test_graphics_draw_core__cleanup(void) {
  free(fb);
}

// Helpers
////////////////////////////////////

// HORIZONTAL LINE HELPERS

typedef void (*HLinePatternDrawFunction)(GContext *ctx, int16_t y, int16_t x1, int16_t x2,
                                         GColor color);

static void prv_draw_hlines_in_rect(GContext *ctx, HLinePatternDrawFunction draw_func,
                                    const GRect *rect, GColor color) {
  for (int16_t y = 0; y < rect->size.h; y++) {
    draw_func(ctx, rect->origin.y + y, rect->origin.x , grect_get_max_x(rect) - 1, color);
  }
}

static void prv_draw_hline_test_pattern(GContext *ctx, HLinePatternDrawFunction draw_func) {
  const GRect *bitmap_bounds = &ctx->dest_bitmap.bounds;
  // Fill the screen with red
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, bitmap_bounds);

  const GSize *bitmap_bounds_size = &bitmap_bounds->size;

  // Draw the bottom rectangle blue with 66% opacity
  // (will appear at the top because data rows are vertically flipped)
  const GSize bottom_rect_size = GSize(bitmap_bounds_size->w * 4 / 5,  bitmap_bounds_size->h / 2);
  const GRect bottom_rect = (GRect) {
    .origin = GPoint((bitmap_bounds_size->w - bottom_rect_size.w) / 2, bitmap_bounds_size->h / 2),
    .size = bottom_rect_size
  };
  GColor bottom_rect_color = GColorBlue;
  bottom_rect_color.a = 2;
  prv_draw_hlines_in_rect(ctx, draw_func, &bottom_rect, bottom_rect_color);

  // Draw the top rectangles green with 66% opacity
  // (will appear at the bottom because data rows are vertically flipped)
  const GSize top_rects_size = GSize(bitmap_bounds_size->w / 4, bitmap_bounds_size->h * 2 / 5);
  const int16_t top_rects_x_offset = ((bitmap_bounds_size->w / 2) - top_rects_size.w) / 2;
  GColor top_rects_color = GColorGreen;
  top_rects_color.a = 2;
  const GRect top_left_rect = (GRect) {
    .origin = GPoint(top_rects_x_offset, 0),
    .size = top_rects_size
  };
  prv_draw_hlines_in_rect(ctx, draw_func, &top_left_rect, top_rects_color);
  const GRect top_right_rect = (GRect) {
    .origin = GPoint((bitmap_bounds_size->w / 2) + top_rects_x_offset, 0),
    .size = top_rects_size
  };
  prv_draw_hlines_in_rect(ctx, draw_func, &top_right_rect, top_rects_color);
}

// VERTICAL LINE HELPERS

typedef void (*VLinePatternDrawFunction)(GContext *ctx, int16_t x, int16_t y1, int16_t y2,
                                         GColor color);

static void prv_draw_vlines_in_rect(GContext *ctx, HLinePatternDrawFunction draw_func,
                                    const GRect *rect, GColor color) {
  for (int16_t x = 0; x < rect->size.w; x++) {
    draw_func(ctx, rect->origin.x + x, rect->origin.y, grect_get_max_y(rect) - 1, color);
  }
}

static void prv_draw_vline_test_pattern(GContext *ctx, HLinePatternDrawFunction draw_func) {
  const GRect *bitmap_bounds = &ctx->dest_bitmap.bounds;
  // Fill the screen with red
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, bitmap_bounds);

  const GSize *bitmap_bounds_size = &bitmap_bounds->size;

  // Draw the left rectangle blue with 66% opacity
  // (will appear at the top because data rows are vertically flipped)
  const GRect left_rect = GRect(0, 0, bitmap_bounds_size->w * 2 / 5,  bitmap_bounds_size->h);
  GColor left_rect_color = GColorBlue;
  left_rect_color.a = 2;
  prv_draw_vlines_in_rect(ctx, draw_func, &left_rect, left_rect_color);

  // Draw the right rectangles green with 66% opacity (top) and yellow with 66% opacity (bottom)
  // (green will appear on top and yellow on bottom because data rows are vertically flipped)
  const GSize right_rects_size = GSize(bitmap_bounds_size->w * 2 / 5, bitmap_bounds_size->h / 4);
  const int16_t right_rects_x = bitmap_bounds->size.w * 3 / 5;
  const int16_t right_rects_y_offset = ((bitmap_bounds_size->h / 2) - right_rects_size.h) / 2;
  GColor top_right_rect_color = GColorGreen;
  top_right_rect_color.a = 2;
  const GRect top_left_rect = (GRect) {
    .origin = GPoint(right_rects_x, right_rects_y_offset),
    .size = right_rects_size
  };
  prv_draw_vlines_in_rect(ctx, draw_func, &top_left_rect, top_right_rect_color);
  GColor bottom_right_rect_color = GColorYellow;
  bottom_right_rect_color.a = 2;
  const GRect top_right_rect = (GRect) {
    .origin = GPoint(right_rects_x, (bitmap_bounds_size->h / 2) + right_rects_y_offset),
    .size = right_rects_size
  };
  prv_draw_vlines_in_rect(ctx, draw_func, &top_right_rect, bottom_right_rect_color);
}

// Tests
////////////////////////////////////

// HORIZONTAL LINE TESTS
// These tests use a pattern of two skinny 66% opacity green rectangles drawn at the top of the
// screen and one wide 66% opacity blue rectangle drawn at the bottom of the screen when drawing
// horizontal lines. Due to the fake GBitmap data row handling, the result you should see is that
// the pattern is clipped to a diamond mask and flipped vertically (i.e. blue rect on top, green
// rects on bottom)

void prv_assign_horizontal_line_raw(GBitmap *framebuffer, int16_t y, Fixed_S16_3 x1,
                                    Fixed_S16_3 x2, GColor color);

static void prv_hline_pattern_assign_horizontal_line_raw(GContext *ctx, int16_t y, int16_t x1,
                                                         int16_t x2, GColor color) {
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) { .integer = x1 };
  const Fixed_S16_3 x2_fixed = (Fixed_S16_3) { .integer = x2 };
  prv_assign_horizontal_line_raw(&ctx->dest_bitmap, y, x1_fixed, x2_fixed, color);
}

void test_graphics_draw_core__assign_horizontal_line_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  prv_draw_hline_test_pattern(&ctx, prv_hline_pattern_assign_horizontal_line_raw);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          TEST_NAMED_PBI_FILE("draw_core_assign_horizontal_line_raw")));
};

void prv_blend_horizontal_line_raw(GBitmap *framebuffer, int16_t y, int16_t x1, int16_t x2,
                                   GColor color);

static void prv_hline_pattern_blend_horizontal_line_raw(GContext *ctx, int16_t y, int16_t x1,
                                                        int16_t x2, GColor color) {
  prv_blend_horizontal_line_raw(&ctx->dest_bitmap, y, x1, x2, color);
}

void test_graphics_draw_core__blend_horizontal_line_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  prv_draw_hline_test_pattern(&ctx, prv_hline_pattern_blend_horizontal_line_raw);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          TEST_NAMED_PBI_FILE("draw_core_blend_horizontal_line_raw")));
};

void prv_assign_horizontal_line_delta_raw(GBitmap *framebuffer, int16_t y,
                                          Fixed_S16_3 x1, Fixed_S16_3 x2,
                                          uint8_t left_aa_offset, uint8_t right_aa_offset,
                                          int16_t clip_box_min_x, int16_t clip_box_max_x,
                                          GColor color);

static void prv_hline_pattern_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                                               int16_t x1, int16_t x2,
                                                               GColor color) {
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) { .integer = x1 };
  Fixed_S16_3 x2_fixed = (Fixed_S16_3) { .integer = x2 };
  const uint8_t gradient_width = (x2 - x1) / 6;
  x2_fixed.integer -= gradient_width;
  prv_assign_horizontal_line_delta_raw(&ctx->dest_bitmap, y, x1_fixed, x2_fixed, gradient_width,
                                       gradient_width, ctx->draw_state.clip_box.origin.x,
                                       grect_get_max_x(&ctx->draw_state.clip_box) - 1, color);
}

void test_graphics_draw_core__assign_horizontal_line_delta_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  prv_draw_hline_test_pattern(&ctx, prv_hline_pattern_assign_horizontal_line_delta_raw);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          TEST_NAMED_PBI_FILE("draw_core_assign_horizontal_line_delta_raw")));
};

// VERTICAL LINE TESTS
// These tests use a pattern of two skinny 66% opacity rectangles drawn at the right of the
// screen (green on top and yellow on bottom) and one tall 66% opacity blue rectangle drawn at the
// left of the screen when drawing vertical lines. Due to the fake GBitmap data row handling, the
// result you should see is that the pattern is clipped to a diamond mask and flipped vertically
// (i.e. green rect on bottom and yellow rect on top) EXCEPT for the prv_assign_vertical_line_raw()
// unit test which disables the vertical flipping

void prv_assign_vertical_line_raw(GBitmap *framebuffer, int16_t x, Fixed_S16_3 y1, Fixed_S16_3 y2,
                                  GColor color);

static void prv_vline_pattern_assign_vertical_line_raw(GContext *ctx, int16_t x, int16_t y1,
                                                       int16_t y2, GColor color) {
  const Fixed_S16_3 y1_fixed = (Fixed_S16_3) { .integer = y1 };
  const Fixed_S16_3 y2_fixed = (Fixed_S16_3) { .integer = y2 };
  prv_assign_vertical_line_raw(&ctx->dest_bitmap, x, y1_fixed, y2_fixed, color);
}

void test_graphics_draw_core__assign_vertical_line_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  prv_draw_vline_test_pattern(&ctx, prv_vline_pattern_assign_vertical_line_raw);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          TEST_NAMED_PBI_FILE("draw_core_assign_vertical_line_raw")));
};

void prv_blend_vertical_line_raw(GBitmap *framebuffer, int16_t x, int16_t y1, int16_t y2,
                                 GColor color);

static void prv_vline_pattern_blend_vertical_line_raw(GContext *ctx, int16_t x, int16_t y1,
                                                      int16_t y2, GColor color) {
  prv_blend_vertical_line_raw(&ctx->dest_bitmap, x, y1, y2, color);
}

void test_graphics_draw_core__blend_vertical_line_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  prv_draw_vline_test_pattern(&ctx, prv_vline_pattern_blend_vertical_line_raw);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap,
                          TEST_NAMED_PBI_FILE("draw_core_blend_vertical_line_raw")));
};

// PIXEL DRAWING AND COLUMN REPLICATION TESTS

void prv_replicate_column_row_raw(GBitmap *framebuffer, int16_t src_x, int16_t dst_x1,
                                  int16_t dst_x2);
void set_pixel_raw_8bit(GContext* ctx, GPoint point);

void test_graphics_draw_core__set_pixel_raw_8bit_replicate_column_row_raw(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  // Draw a colored gradient of horizontal lines down the left half of the screen
  const GRect bitmap_bounds = ctx.dest_bitmap.bounds;
  const uint8_t max_rgb_value = 0b00111111;
  for (int y = 0; y < bitmap_bounds.size.h; y++) {
    GColor color = (GColor) {
      .argb = y * max_rgb_value / (bitmap_bounds.size.h - 1)
    };
    color.a = 3; // 100% opacity
    ctx.draw_state.stroke_color = color;
    for (int x = 0; x < bitmap_bounds.size.w / 2; x++) {
      const GPoint point = GPoint(x, y);
      set_pixel_raw_8bit(&ctx, point);
    }
  }

  // Replicate the last column of the colored gradient for the remaining columns of the bitmap
  prv_replicate_column_row_raw(&ctx.dest_bitmap,
                               (bitmap_bounds.size.w / 2) - 1,
                               (bitmap_bounds.size.w / 2),
                               bitmap_bounds.size.w - 1);

  const bool result =
    gbitmap_pbi_eq(&ctx.dest_bitmap,
                   TEST_NAMED_PBI_FILE("draw_core_set_pixel_raw_8bit_replicate_column_row_raw"));
  cl_check(result);
};

void test_graphics_draw_core__plot_pixel(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, false, 1);

  const GRect *bitmap_bounds = &ctx.dest_bitmap.bounds;

  // Fill the screen with red
  graphics_context_set_fill_color(&ctx, GColorRed);
  graphics_fill_rect(&ctx, bitmap_bounds);

  // Draw 66% opacity blue pixels using graphics_private_plot_pixel() over the entire screen
  // The expected result is that the entire screen will be purple due to the blending
  for (int y = 0; y < bitmap_bounds->size.h; y++) {
    for (int x = 0; x < bitmap_bounds->size.w; x++) {
      graphics_private_plot_pixel(&ctx.dest_bitmap, &ctx.draw_state.clip_box, x, y, 1, GColorBlue);
    }
  }

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, TEST_NAMED_PBI_FILE("draw_core_plot_pixel")));
};

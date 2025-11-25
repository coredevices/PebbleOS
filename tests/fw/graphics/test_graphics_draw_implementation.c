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

// State
////////////////////////////////////

typedef enum {
  GDrawRawFunctionTypeAssignHorizontalLine,
  GDrawRawFunctionTypeAssignVerticalLine,
  GDrawRawFunctionTypeBlendHorizontalLine,
  GDrawRawFunctionTypeBlendVerticalLine,
  GDrawRawFunctionTypeAssignHorizontalLineDelta,
  NumGDrawRawFunctionTypes
} GDrawRawFunctionType;

static unsigned int s_raw_drawing_function_counters[NumGDrawRawFunctionTypes];

// Setup and Teardown
////////////////////////////////////

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_draw_implementation__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) { DISP_COLS, DISP_ROWS });
  memset(s_raw_drawing_function_counters, 0, sizeof(s_raw_drawing_function_counters));
}

// Teardown
void test_graphics_draw_implementation__cleanup(void) {
  free(fb);
}

// Fake raw drawing functions
////////////////////////////////////

static void prv_fake_raw_assign_horizontal_line(GContext *ctx, int16_t y, Fixed_S16_3 x1,
                                                Fixed_S16_3 x2, GColor color) {
  s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignHorizontalLine]++;
}

static void prv_fake_raw_assign_vertical_line(GContext *ctx, int16_t x, Fixed_S16_3 y1,
                                              Fixed_S16_3 y2, GColor color) {
  s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignVerticalLine]++;
}

static void prv_fake_raw_blend_horizontal_line(GContext *ctx, int16_t y, int16_t x1,
                                               int16_t x2, GColor color) {
  s_raw_drawing_function_counters[GDrawRawFunctionTypeBlendHorizontalLine]++;
}

static void prv_fake_raw_blend_vertical_line(GContext *ctx, int16_t x, int16_t y1,
                                             int16_t y2, GColor color) {
  s_raw_drawing_function_counters[GDrawRawFunctionTypeBlendVerticalLine]++;
}

static void prv_fake_raw_assign_horizontal_line_delta(GContext *ctx, int16_t y,
                                                      Fixed_S16_3 x1, Fixed_S16_3 x2,
                                                      uint8_t left_aa_offset,
                                                      uint8_t right_aa_offset,
                                                      int16_t clip_box_min_x,
                                                      int16_t clip_box_max_x, GColor color) {
  s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignHorizontalLineDelta]++;
}

// Tests
////////////////////////////////////

#define CLIP_RECT_DRAW_BOX GRect(0, 0, DISP_COLS, DISP_ROWS)
#define CLIP_RECT_CLIP_BOX GRect(0, 0, DISP_COLS, DISP_ROWS)

void test_graphics_draw_implementation__fill_circle_aa(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  setup_test_aa_sw(&ctx, fb, CLIP_RECT_CLIP_BOX, CLIP_RECT_DRAW_BOX, true, 1);

  // Set the draw implementation functions to the fake ones in this file that just increment some
  // counters, then call graphics_fill_circle() and check the values of the counters

  ctx.draw_state.draw_implementation = &(GDrawRawImplementation) {
    .assign_horizontal_line = prv_fake_raw_assign_horizontal_line,
    .assign_vertical_line = prv_fake_raw_assign_vertical_line,
    .blend_horizontal_line = prv_fake_raw_blend_horizontal_line,
    .blend_vertical_line = prv_fake_raw_blend_vertical_line,
    .assign_horizontal_line_delta = prv_fake_raw_assign_horizontal_line_delta,
  };

  const GRect bounds = ctx.dest_bitmap.bounds;

  graphics_context_set_fill_color(&ctx, GColorBlack);
  graphics_fill_circle(&ctx, grect_center_point(&bounds), 5);

  cl_assert_equal_i(11, s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignHorizontalLine]);
  cl_assert_equal_i(0, s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignVerticalLine]);
  cl_assert_equal_i(0, s_raw_drawing_function_counters[GDrawRawFunctionTypeBlendHorizontalLine]);
  cl_assert_equal_i(0, s_raw_drawing_function_counters[GDrawRawFunctionTypeBlendVerticalLine]);
  cl_assert_equal_i(0,
                    s_raw_drawing_function_counters[GDrawRawFunctionTypeAssignHorizontalLineDelta]);
};

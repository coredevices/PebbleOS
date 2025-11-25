/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/perimeter.h"

#include "clar.h"
#include "util/trig.h"

#include <string.h>
#include <stdio.h>

#include <math.h>
#ifndef M_PI
// M_PI doesn't exist in Linux
#define M_PI 3.14159265358979323846	/* pi */
#endif
#define DEG2RAD(a) (M_PI/180*(a))

// Stubs
////////////////////////////////////
#include "stubs_heap.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"

#define BETWEEN(val, low, high) \
  (val >= low && val <= high) ? true : false

// Tests
////////////////////////////////////

GRangeHorizontal perimeter_for_circle(GRangeVertical vertical_range, GPoint center, int32_t radius);
GRangeHorizontal perimeter_for_display_round(const GPerimeter *perimeter,
                                             const GSize *ctx_size,
                                             GRangeVertical vertical_range,
                                             uint16_t inset);
GRangeHorizontal perimeter_for_display_rect(const GPerimeter *perimeter,
                                            const GSize *ctx_size,
                                            GRangeVertical vertical_range,
                                            uint16_t inset);

void test_perimeter__perimeter_for_circle(void) {
  GRect bounds = GRect(0,0,180,180);
  GPoint center = grect_center_point(&bounds);
  int16_t radius = bounds.size.w / 2;

  // test robustness of perimeter_horizontal_range_for_circle
  for (int y = 0; y < bounds.size.h; y++) {
    GRangeHorizontal h_range = perimeter_for_circle(
      (GRangeVertical) {.origin_y = y, .size_h = 0}, center, radius);
    
    // internally we use integer_sqrt, which causes precision loss
    // so we need to mirror some of the fixed point truncation here
    int16_t height = 90 - y;
    int16_t width = sqrt(radius * radius - height * height);
    int16_t test_origin = 90 - width;

    // Due to integer math and truncation, we need to test +-1
    cl_assert(BETWEEN(h_range.origin_x, test_origin - 1, test_origin + 1));
    cl_assert(BETWEEN(h_range.size_w, (width - 1) * 2, (width + 1) * 2));
  }
}

#define cl_assert_equal_rangehorizontal(r1, r2) \
  do { \
	  cl_assert_equal_i((r1).origin_x, (r2).origin_x); \
	  cl_assert_equal_i((r1).size_w, (r2).size_w); \
  } while(0)

void test_perimeter__perimeter_for_display_rect(void) {
  GPerimeter p = {
    .callback = perimeter_for_display_rect,
  };
  const GSize ctx_size = GSize(DISP_COLS, DISP_ROWS);
  GRangeVertical r = {.origin_y = 10, .size_h = 10};

  GRangeHorizontal expected = (GRangeHorizontal){.origin_x = 0, .size_w = DISP_COLS};
  cl_assert_equal_rangehorizontal(expected, perimeter_for_display_rect(&p, &ctx_size, r, 0));
  expected = (GRangeHorizontal){.origin_x = 5, .size_w = DISP_COLS - 10};
  cl_assert_equal_rangehorizontal(expected, perimeter_for_display_rect(&p, &ctx_size, r, 5));
  cl_assert_equal_i(0, perimeter_for_display_rect(&p, &ctx_size, r, 500).size_w);
}

void test_perimeter__perimeter_for_display_round(void) {
  GPerimeter p = {
    .callback = perimeter_for_display_round,
  };
  GRangeVertical r = {.origin_y = 10, .size_h = 10};
  const GSize ctx_size = GSize(DISP_COLS, DISP_ROWS);
  const GRect disp = GRect(0, 0, DISP_COLS, DISP_ROWS);

  GRangeHorizontal expected =
    perimeter_for_circle(r, grect_center_point(&disp), grect_shortest_side(disp) / 2);
  cl_assert_equal_rangehorizontal(expected, perimeter_for_display_round(&p, &ctx_size, r, 0));
  expected = perimeter_for_circle(r, grect_center_point(&disp), grect_shortest_side(disp) / 2 - 5);
  cl_assert_equal_rangehorizontal(expected, perimeter_for_display_round(&p, &ctx_size, r, 5));
  cl_assert_equal_i(0, perimeter_for_display_round(&p, &ctx_size, r, 500).size_w);
}

void test_perimeter__g_perimeter_for_display(void) {
  GPerimeterCallback expected = PBL_IF_RECT_ELSE(perimeter_for_display_rect,
                                                 perimeter_for_display_round);
  cl_assert_equal_p(expected, g_perimeter_for_display->callback);
}

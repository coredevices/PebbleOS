/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/gtypes.h"

#include "clar.h"
#include "pebble_asserts.h"

#include <stdio.h>

// stubs
#include "stubs_process_manager.h"
#include "stubs_passert.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"

/////////////////////////////

void test_gtypes__gpoint_scale_by_gsize(void) {
  GSize from = GSize(10, 20);
  GSize to = GSize(20, 40);
  GPoint point = GPoint(10, 10);
  GPoint result = GPointZero;

  result = gpoint_scale_by_gsize(point, from, to);
  cl_assert_equal_i(result.x, 20);
  cl_assert_equal_i(result.y, 20);
}

void test_gtypes__gpoint_scale_by_gsize_from_zero(void) {
  GSize from = GSizeZero;
  GSize to = GSize(20, 40);
  GPoint point = GPoint(10, 10);
  GPoint result = GPointZero;

  result = gpoint_scale_by_gsize(point, from, to);
  cl_assert_equal_i(result.x, 0);
  cl_assert_equal_i(result.y, 0);
}

void test_gtypes__gpoint_scale_by_gsize_to_zero(void) {
  GSize from = GSize(10, 20);
  GSize to = GSizeZero;
  GPoint point = GPoint(10, 10);
  GPoint result = GPointZero;

  result = gpoint_scale_by_gsize(point, from, to);
  cl_assert_equal_i(result.x, 0);
  cl_assert_equal_i(result.y, 0);
}

#define cl_assert_equal_insets(i1, i2) \
  do { \
	  cl_assert_equal_i(i1.top, i2.top); \
	  cl_assert_equal_i(i1.right, i2.right); \
	  cl_assert_equal_i(i1.bottom, i2.bottom); \
	  cl_assert_equal_i(i1.left, i2.left); \
  } while(0)

void test_gtypes__gedge_insets(void) {
  GEdgeInsets g4 = (GEdgeInsets){.top = 1, .right = 2, .bottom = 3, .left = 4};
  cl_assert_equal_insets(GEdgeInsets(1, 2, 3, 4), g4);

  GEdgeInsets g3 = (GEdgeInsets){.top = 1, .right = 2, .bottom = 3, .left = 2};
  cl_assert_equal_insets(GEdgeInsets(1, 2, 3), g3);

  GEdgeInsets g2 = (GEdgeInsets){.top = 1, .right = 2, .bottom = 1, .left = 2};
  cl_assert_equal_insets(GEdgeInsets(1, 2), g2);

  GEdgeInsets g1 = (GEdgeInsets){.top = 1, .right = 1, .bottom = 1, .left = 1};
  cl_assert_equal_insets(GEdgeInsets(1), g1);
}

void test_gtypes__grect_longest_side(void) {
  cl_assert_equal_i(0, grect_longest_side(GRectZero));
  cl_assert_equal_i(20, grect_longest_side(GRect(0, 0, 10, 20)));
  cl_assert_equal_i(20, grect_longest_side(GRect(0, 0, 20, 10)));
  cl_assert_equal_i(20, grect_longest_side(GRect(0, 0, 10, -20)));
  cl_assert_equal_i(20, grect_longest_side(GRect(0, 0, -20, 10)));
}

void test_gtypes__grect_shortest_side(void) {
  cl_assert_equal_i(0, grect_shortest_side(GRectZero));
  cl_assert_equal_i(10, grect_shortest_side(GRect(0, 0, 10, 20)));
  cl_assert_equal_i(10, grect_shortest_side(GRect(0, 0, 20, 10)));
  cl_assert_equal_i(10, grect_shortest_side(GRect(0, 0, 10, -20)));
  cl_assert_equal_i(10, grect_shortest_side(GRect(0, 0, -20, 10)));
}

void test_gtypes__grect_inset(void) {
  GRect rect = GRect(10, 20, 30, 40);
  cl_assert_equal_grect(GRect(12, 23, 26, 34), grect_inset_internal(rect, 2, 3));
  cl_assert_equal_grect(GRect(7,  18, 36, 44), grect_inset_internal(rect, -3, -2));
}

void test_gtypes__grect_inset_standardizes(void) {
  GRect rect = GRect(100, 100, -30, -40);
  cl_assert_equal_grect(GRect(70, 60, 30, 40), grect_inset_internal(rect, 0, 0));
  cl_assert_equal_grect(GRect(72, 63, 26, 34), grect_inset_internal(rect, 2, 3));
}

void test_gtypes__grect_inset_returns_zero_rect_for_large_insets(void) {
  GRect rect = GRect(10, 20, 30, 40);
  cl_assert_equal_grect(GRect(25, 20, 0, 40), grect_inset_internal(rect, 15, 0));
  cl_assert_equal_grect(GRectZero, grect_inset_internal(rect, 16, 0));

  cl_assert_equal_grect(GRect(10, 40, 30, 0), grect_inset_internal(rect, 0, 20));
  cl_assert_equal_grect(GRectZero, grect_inset_internal(rect, 0, 21));

  cl_assert_equal_grect(GRectZero, grect_inset_internal(rect, 16, 21));
}

void test_gtypes__grect_crop_asserts_for_large_insets(void) {
  GRect rect = GRect(10, 20, 30, 40);
  cl_assert_equal_grect(GRect(25, 35, 0, 10), grect_crop(rect, 15));
  cl_assert_passert(grect_crop(rect, 16));
}

void test_gtypes__pbl_if_rect_else(void) {
#if defined(PLATFORM_TINTIN)
  cl_assert_equal_i(1, PBL_IF_RECT_ELSE(1,2));
#elif defined(PLATFORM_SNOWY)
  cl_assert_equal_i(1, PBL_IF_RECT_ELSE(1,2));
#elif defined(PLATFORM_SPALDING)
  cl_assert_equal_i(2, PBL_IF_RECT_ELSE(1,2));
#else
#error "unknown platform"
#endif
}

void test_gtypes__pbl_if_round_else(void) {
#if defined(PLATFORM_TINTIN)
  cl_assert_equal_i(2, PBL_IF_ROUND_ELSE(1,2));
#elif defined(PLATFORM_SNOWY)
  cl_assert_equal_i(2, PBL_IF_ROUND_ELSE(1,2));
#elif defined(PLATFORM_SPALDING)
  cl_assert_equal_i(1, PBL_IF_ROUND_ELSE(1,2));
#else
#error "unknown platform"
#endif
}

void test_gtypes__pbl_if_bw_else(void) {
#if defined(PLATFORM_TINTIN)
  cl_assert_equal_i(1, PBL_IF_BW_ELSE(1,2));
#elif defined(PLATFORM_SNOWY)
  cl_assert_equal_i(2, PBL_IF_BW_ELSE(1,2));
#elif defined(PLATFORM_SPALDING)
  cl_assert_equal_i(2, PBL_IF_BW_ELSE(1,2));
#else
#error "unknown platform"
#endif
}

void test_gtypes__pbl_if_color_else(void) {
#if defined(PLATFORM_TINTIN)
  cl_assert_equal_i(2, PBL_IF_COLOR_ELSE(1,2));
#elif defined(PLATFORM_SNOWY)
  cl_assert_equal_i(1, PBL_IF_COLOR_ELSE(1,2));
#elif defined(PLATFORM_SPALDING)
  cl_assert_equal_i(1, PBL_IF_COLOR_ELSE(1,2));
#else
#error "unknown platform"
#endif
}

void test_gtypes__color_fallback(void) {
#if defined(PLATFORM_TINTIN)
  cl_assert_equal_i(2, COLOR_FALLBACK(1,2));
#elif defined(PLATFORM_SNOWY)
  cl_assert_equal_i(1, COLOR_FALLBACK(1,2));
#elif defined(PLATFORM_SPALDING)
  cl_assert_equal_i(1, COLOR_FALLBACK(1,2));
#else
#error "unknown platform"
#endif
}

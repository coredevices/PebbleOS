/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics_circle.h"
#include "util/trig.h"

#include "clar.h"
#include "pebble_asserts.h"

#include <stdio.h>

// stubs
#include "stubs_process_manager.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"

GBitmap* graphics_capture_frame_buffer(GContext* ctx) {
  return NULL;
}
bool graphics_release_frame_buffer(GContext* ctx, GBitmap* buffer) {
  return true;
}

void graphics_draw_pixel(){}
void graphics_fill_rect(GContext* ctx, const GRect *rect) {}
void graphics_private_draw_horizontal_line(){}
void graphics_private_draw_vertical_line(){}
void graphics_private_plot_pixel(){}
void graphics_private_set_pixel(){}

/////////////////////////////


static GPointPrecise s_center;
static Fixed_S16_3 s_radius;
static Fixed_S16_3 s_radius_inner;
static Fixed_S16_3 s_radius_outer;
static int32_t s_angle_start;
static int32_t s_angle_end;

////////////////////////////

void test_graphics_circle__initialize(void) {
  s_center = (GPointPrecise){};
  s_radius = (Fixed_S16_3){};
  s_radius_inner = (Fixed_S16_3){};
  s_radius_outer = (Fixed_S16_3){};
}

void test_graphics_circle__gpoint_from_polar_returns_zero_for_null(void) {
  const uint16_t radius = 5;
  const GPoint result = gpoint_from_polar_internal(NULL, radius, 0);
  cl_assert_equal_gpoint(result, GPointZero);
}

void test_graphics_circle__gpoint_from_polar_returns_correct_points(void) {
  const uint16_t radius = 5;
  GPoint result;

  const GPoint origin_center = GPointZero;
  // 90 degrees should be (5, 0)
  result = gpoint_from_polar_internal(&origin_center, radius, (TRIG_MAX_ANGLE / 4));
  cl_assert_equal_gpoint(result, GPoint(5, 0));
  // 270 (90 * 3) degrees should be (-5, 0)
  result = gpoint_from_polar_internal(&origin_center, radius, (TRIG_MAX_ANGLE * 3 / 4));
  cl_assert_equal_gpoint(result, GPoint(-5, 0));

  const GPoint offset_center = GPoint(1, 1);
  // 90 degrees should be (6, 1)
  result = gpoint_from_polar_internal(&offset_center, radius, (TRIG_MAX_ANGLE / 4));
  cl_assert_equal_gpoint(result, GPoint(6, 1));
  // 270 (90 * 3) degrees should be (-4, 1)
  result = gpoint_from_polar_internal(&offset_center, radius, (TRIG_MAX_ANGLE * 3 / 4));
  cl_assert_equal_gpoint(result, GPoint(-4, 1));
}

void test_graphics_circle__gpoint_from_polar_normalizes_input_angles(void) {
  const uint16_t radius = 5;
  const GPoint center = GPointZero;

  GPoint result;

  // -180 degrees should be (0, 5)
  result = gpoint_from_polar_internal(&center, radius, -(TRIG_MAX_ANGLE / 2));
  cl_assert_equal_gpoint(result, GPoint(0, 5));

  // -90 degrees should be (-5, 0)
  result = gpoint_from_polar_internal(&center, radius, -(TRIG_MAX_ANGLE / 4));
  cl_assert_equal_gpoint(result, GPoint(-5, 0));

  // -450 degrees (-90 * 5 -> -90) should be (-5, 0)
  result = gpoint_from_polar_internal(&center, radius, -(TRIG_MAX_ANGLE * 5 / 4));
  cl_assert_equal_gpoint(result, GPoint(-5, 0));

  // 450 degrees (90 * 5 -> 90) should be (0, 5)
  result = gpoint_from_polar_internal(&center, radius, (TRIG_MAX_ANGLE * 5 / 4));
  cl_assert_equal_gpoint(result, GPoint(5, 0));

  // 360 degrees (-> 0 degrees) should be (0, -5)
  result = gpoint_from_polar_internal(&center, radius, TRIG_MAX_ANGLE);
  cl_assert_equal_gpoint(result, GPoint(0, -5));

  // -360 degrees (-> 0 degrees) should be (0, -5)
  result = gpoint_from_polar_internal(&center, radius, -TRIG_MAX_ANGLE);
  cl_assert_equal_gpoint(result, GPoint(0, -5));
}

void test_graphics_circle__gpoint_from_polar_correct_scale(void) {
  // edge cases are covered above, this test only verifies that
  // the internal implementation correctly scales
  GPoint result = gpoint_from_polar(GRect(0, 0, 10, 10), GOvalScaleModeFillCircle, 0);
  cl_assert_equal_gpoint(result, GPoint(4, 0));
}

void test_graphics_circle__grect_centered_from_polar(void) {
  GOvalScaleMode mode = GOvalScaleModeFillCircle;

  // Basic container rect with GPointZero origin
  const GRect container_rect1 = GRect(0, 0, 10, 10);
  const GRect resulting_rect1 = grect_centered_from_polar(container_rect1, mode, 0, GSize(3, 5));
  cl_assert_equal_grect(resulting_rect1, GRect(3, -2, 3, 5));

  // Container rect is offset from GPointZero
  const GRect container_rect2 = GRect(2, 2, 4, 4);
  const GRect resulting_rect2 = grect_centered_from_polar(container_rect2, mode, 0, GSize(2, 4));
  cl_assert_equal_grect(resulting_rect2, GRect(3, 0, 2, 4));

  // Odd-length width and height for container rect, 180 degree angle
  const GRect container_rect3 = GRect(2, 2, 5, 5);
  const GRect resulting_rect3 = grect_centered_from_polar(container_rect3, mode,
                                                          DEG_TO_TRIGANGLE(180), GSize(2, 4));
  cl_assert_equal_grect(resulting_rect3, GRect(3, 4, 2, 4));
}

void test_graphics_circle__grect_centered_internal(void) {
  GPointPrecise p1 = GPointPrecise(0, 0);
  // GRectZero + standardize
  cl_assert_equal_grect(GRect(0, 0, 0, 0), grect_centered_internal(&p1, GSize(0, 0)));
  cl_assert_equal_grect(GRect(0, -1, 1, 2), grect_centered_internal(&p1, GSize(-1, -2)));

  // handles fixed point
  cl_assert_equal_grect(GRect(-1, -1, 2, 2), grect_centered_internal(&p1, GSize(2, 2)));
  p1.x.raw_value += FIXED_S16_3_HALF.raw_value;
  cl_assert_equal_grect(GRect(0, -1, 2, 2), grect_centered_internal(&p1, GSize(2, 2)));

  // Repeat for an offset center point
  GPointPrecise p2 = GPointPreciseFromGPoint(GPoint(5, 5));

  // GRectZero + standardize
  cl_assert_equal_grect(GRect(5, 5, 0, 0), grect_centered_internal(&p2, GSize(0, 0)));
  cl_assert_equal_grect(GRect(5, 4, 1, 2), grect_centered_internal(&p2, GSize(-1, -2)));

  // handles fixed point
  cl_assert_equal_grect(GRect(4, 4, 2, 2), grect_centered_internal(&p2, GSize(2, 2)));
  p2.x.raw_value += FIXED_S16_3_HALF.raw_value;
  cl_assert_equal_grect(GRect(5, 4, 2, 2), grect_centered_internal(&p2, GSize(2, 2)));

  // Repeat for a positive offset center point with 0.5 fractions
  GPointPrecise p3 = GPointPreciseFromGPoint(GPoint(5, 5));
  p3.x.raw_value += FIXED_S16_3_HALF.raw_value;
  p3.y.raw_value += FIXED_S16_3_HALF.raw_value;

  // GRectZero + standardize
  cl_assert_equal_grect(GRect(6, 6, 0, 0), grect_centered_internal(&p3, GSize(0, 0)));
  cl_assert_equal_grect(GRect(5, 5, 1, 2), grect_centered_internal(&p3, GSize(-1, -2)));

  // handles fixed point
  cl_assert_equal_grect(GRect(5, 5, 2, 2), grect_centered_internal(&p3, GSize(2, 2)));
  p3.x.raw_value += FIXED_S16_3_HALF.raw_value;
  cl_assert_equal_grect(GRect(5, 5, 2, 2), grect_centered_internal(&p3, GSize(2, 2)));

  // Repeat for a negative offset center point with 0.5 fractions
  GPointPrecise p4 = GPointPreciseFromGPoint(GPoint(-5, -5));
  p4.x.raw_value -= FIXED_S16_3_HALF.raw_value;
  p4.y.raw_value -= FIXED_S16_3_HALF.raw_value;

  // GRectZero + standardize
  cl_assert_equal_grect(GRect(-5, -5, 0, 0), grect_centered_internal(&p4, GSize(0, 0)));
  cl_assert_equal_grect(GRect(-6, -6, 1, 2), grect_centered_internal(&p4, GSize(-1, -2)));

  // handles fixed point
  cl_assert_equal_grect(GRect(-6, -6, 2, 2), grect_centered_internal(&p4, GSize(2, 2)));
  p4.x.raw_value += FIXED_S16_3_HALF.raw_value;
  cl_assert_equal_grect(GRect(-6, -6, 2, 2), grect_centered_internal(&p4, GSize(2, 2)));
}

#define cl_assert_fixedS16_3(v, f) \
  cl_assert_equal_i((v).raw_value, (int)((f) * FIXED_S16_3_ONE.raw_value))

#define cl_assert_gpoint_precise(p, px, py) \
  do { \
    cl_assert_fixedS16_3(p.x, px); \
    cl_assert_fixedS16_3(p.y, py); \
  } while(0)


void test_graphics_circle__grect_polar_calc_values_handles_null(void) {
  GPointPrecise center = {};
  Fixed_S16_3 radius = {};
  const GRect r = GRect(0, 0, 3, 5);
  const GOvalScaleMode mode = GOvalScaleModeFitCircle;

  grect_polar_calc_values(&r, mode, NULL, NULL);
  grect_polar_calc_values(&r, mode, &center, NULL);
  grect_polar_calc_values(&r, mode, NULL, &radius);

  cl_assert_gpoint_precise(center, 1, 2);
  cl_assert_fixedS16_3(radius, 1);

  grect_polar_calc_values(NULL, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 1, 2);
  cl_assert_fixedS16_3(radius, 1);
}

void test_graphics_circle__grect_polar_calc_values_edge_cases(void) {
  GPointPrecise center = {};
  Fixed_S16_3 radius = {};
  const GOvalScaleMode mode = GOvalScaleModeFillCircle;

  GRect r = GRect(0, 5, 0, 0);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 0, 5);
  cl_assert_fixedS16_3(radius, 0);

  // 1 pixel width means radius of 0
  r = GRect(-1, -5, 1, 1);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, -1, -5);
  cl_assert_fixedS16_3(radius, 0);

  // 2 pixel width means: center is 1px from side, 0.5 pixels to center of outer pixels
  r = GRect(-1, -5, 2, 2);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, -0.5, -4.5);
  cl_assert_fixedS16_3(radius, 0.5);
}

void test_graphics_circle__grect_polar_calc_values_standardizes(void) {
  GPointPrecise center = {};
  Fixed_S16_3 radius = {};
  const GOvalScaleMode mode = GOvalScaleModeFitCircle;

  GRect r = GRect(0, 0, 10, 20);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 4.5, 9.5);
  cl_assert_fixedS16_3(radius, 4.5);

  r = GRect(0, 0, -10, -20);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, -5.5, -10.5);
  cl_assert_fixedS16_3(radius, 4.5);
}

void test_graphics_circle__grect_polar_calc_values_square(void) {
  GPointPrecise center = {};
  Fixed_S16_3 radius = {};
  GRect r = GRect(0, 0, 5, 5);
  GOvalScaleMode mode = GOvalScaleModeFitCircle; // irrelevant as we deal with squares

  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 2, 2);
  cl_assert_fixedS16_3(radius, 2);

  r = GRect(0, 0, 6, 6);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 2.5, 2.5);
  cl_assert_fixedS16_3(radius, 2.5);

  r = GRect(0, 0, 10, 10);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 4.5, 4.5);
  cl_assert_fixedS16_3(radius, 4.5);

  r = GRect(1, 1, 9, 9);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 5, 5);
  cl_assert_fixedS16_3(radius, 4);

  r = GRect(2, 2, 8, 8);
  grect_polar_calc_values(&r, mode, &center, &radius);
  cl_assert_gpoint_precise(center, 5.5, 5.5);
  cl_assert_fixedS16_3(radius, 3.5);
}

void test_graphics_circle__grect_polar_calc_values_mode(void) {
  GPointPrecise center = {};
  Fixed_S16_3 radius = {};
  GRect r = GRect(0, 0, 144, 168);

  grect_polar_calc_values(&r, GOvalScaleModeFitCircle, &center, &radius);
  cl_assert_gpoint_precise(center, 144 / 2 - 0.5, 168 / 2 - 0.5);
  cl_assert_fixedS16_3(radius, 144 / 2 - 0.5);

  grect_polar_calc_values(&r, GOvalScaleModeFillCircle, &center, &radius);
  cl_assert_gpoint_precise(center, 144 / 2 - 0.5, 168 / 2 - 0.5);
  cl_assert_fixedS16_3(radius, 168 / 2 - 0.5);
}

void graphics_draw_arc_precise_internal(GContext *ctx, GPointPrecise center,
                                        Fixed_S16_3 radius,
                                        int32_t angle_start, int32_t angle_end) {
  s_center = center;
  s_radius = radius;
}

void test_graphics_circle__draw_arc(void) {
  cl_assert_gpoint_precise(s_center, 0, 0);
  cl_assert_fixedS16_3(s_radius, 0);

  graphics_draw_arc(NULL, GRect(0, 0, 10, 12), GOvalScaleModeFitCircle, 0, 0);
  cl_assert_gpoint_precise(s_center, 4.5, 5.5);
  cl_assert_fixedS16_3(s_radius, 4.5);
}

void test_graphics_circle__fill_oval(void) {
  cl_assert_gpoint_precise(s_center, 0, 0);
  cl_assert_fixedS16_3(s_radius_inner, 0);
  cl_assert_fixedS16_3(s_radius_outer, 0);
  cl_assert_equal_i(s_angle_start, 0);
  cl_assert_equal_i(s_angle_end, 0);

  graphics_fill_oval(NULL, GRect(0, 0, 10, 12), GOvalScaleModeFitCircle);

  cl_assert_gpoint_precise(s_center, 4.5, 5.5);
  cl_assert_fixedS16_3(s_radius_outer, 4.5);
  cl_assert(s_radius_inner.integer <= 0);
  cl_assert_equal_i(s_angle_start, 0);
  cl_assert_equal_i(s_angle_end, TRIG_MAX_ANGLE);

  graphics_fill_oval(NULL, GRect(10, 12, -10, -12), GOvalScaleModeFitCircle);

  cl_assert_gpoint_precise(s_center, 4.5, 5.5);
  cl_assert_fixedS16_3(s_radius_outer, 4.5);
  cl_assert(s_radius_inner.integer <= 0);
  cl_assert_equal_i(s_angle_start, 0);
  cl_assert_equal_i(s_angle_end, TRIG_MAX_ANGLE);

  graphics_fill_oval(NULL, GRect(0, 0, 0, 0), GOvalScaleModeFillCircle);
  cl_assert_gpoint_precise(s_center, 0.0, 0.0);
  cl_assert_fixedS16_3(s_radius_outer, 0.0);
  cl_assert(s_radius_inner.integer <= 0);
  cl_assert_equal_i(s_angle_start, 0);
  cl_assert_equal_i(s_angle_end, TRIG_MAX_ANGLE);
}

void graphics_fill_radial_precise_internal(GContext *ctx, GPointPrecise center,
                                           Fixed_S16_3 radius_inner, Fixed_S16_3 radius_outer,
                                           int32_t angle_start, int32_t angle_end) {
  s_center = center;
  s_radius_inner = radius_inner;
  s_radius_outer = radius_outer;
  s_angle_start = angle_start;
  s_angle_end = angle_end;
}


void test_graphics_circle__fill_radial(void) {
  GContext *ctx = (GContext *)&ctx;
  cl_assert_gpoint_precise(s_center, 0, 0);
  cl_assert_fixedS16_3(s_radius_inner, 0);
  cl_assert_fixedS16_3(s_radius_outer, 0);

  graphics_fill_radial(NULL, GRect(0, 0, 10, 12), GOvalScaleModeFitCircle, 3, 0, 0);
  cl_assert_gpoint_precise(s_center, 4.5, 5.5);
  cl_assert_fixedS16_3(s_radius_outer, 4.5);
  cl_assert_fixedS16_3(s_radius_inner, 1.5);
}

void test_graphics_circle__DEG_TO_TRIGANGLE(void) {
  cl_assert_equal_i(DEG_TO_TRIGANGLE(720), TRIG_MAX_ANGLE * 2);
  cl_assert_equal_i(DEG_TO_TRIGANGLE(-720), -TRIG_MAX_ANGLE * 2);

  cl_assert_equal_i(DEG_TO_TRIGANGLE(360), TRIG_MAX_ANGLE);
  cl_assert_equal_i(DEG_TO_TRIGANGLE(-360), -TRIG_MAX_ANGLE);

  cl_assert_equal_i(DEG_TO_TRIGANGLE(180), TRIG_PI);
  cl_assert_equal_i(DEG_TO_TRIGANGLE(-180), -TRIG_PI);

  cl_assert_equal_i(DEG_TO_TRIGANGLE(90), TRIG_PI / 2);
  cl_assert_equal_i(DEG_TO_TRIGANGLE(-90), -TRIG_PI / 2);

  cl_assert_equal_i(DEG_TO_TRIGANGLE(0), 0);
}

void test_graphics_circle__TRIGANGLE_TO_DEG(void) {
  cl_assert_equal_i(TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE * 2), 720);
  cl_assert_equal_i(TRIGANGLE_TO_DEG(-TRIG_MAX_ANGLE * 2), -720);

  cl_assert_equal_i(TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE), 360);
  cl_assert_equal_i(TRIGANGLE_TO_DEG(-TRIG_MAX_ANGLE), -360);

  cl_assert_equal_i(TRIGANGLE_TO_DEG(TRIG_PI / 2), 90);
  cl_assert_equal_i(TRIGANGLE_TO_DEG(-TRIG_PI / 2), -90);

  cl_assert_equal_i(TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE / 2), 180);
  cl_assert_equal_i(TRIGANGLE_TO_DEG(-TRIG_MAX_ANGLE / 2), -180);

  cl_assert_equal_i(TRIGANGLE_TO_DEG(TRIG_PI), 180);
  cl_assert_equal_i(TRIGANGLE_TO_DEG(-TRIG_PI), -180);

  cl_assert_equal_i(TRIGANGLE_TO_DEG(0), 0);
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "applib/ui/animation_timing.h"
#include "applib/ui/animation.h"

// stubs
#include "stubs_logging.h"
#include "stubs_passert.h"

// tests

void test_animation_timing__scaled(void) {
  AnimationProgress third = ANIMATION_NORMALIZED_MAX / 3;
  AnimationProgress half = ANIMATION_NORMALIZED_MAX / 2;
  AnimationProgress two_third = ANIMATION_NORMALIZED_MAX * 2 / 3;
  AnimationProgress four_third = ANIMATION_NORMALIZED_MAX * 4 / 3;

  AnimationProgress (*f)(AnimationProgress time_normalized,
                         AnimationProgress interval_start,
                         AnimationProgress interval_end) = animation_timing_scaled;

  cl_assert_equal_i(-half,      f(-half, 0, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(0,          f(0, 0, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(third,      f(third, 0, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(two_third,  f(two_third, 0, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(four_third, f(four_third, 0, ANIMATION_NORMALIZED_MAX));

  cl_assert_equal_i(-65535, f(-half, 0, half));
  cl_assert_equal_i(0,      f(0, 0, half));
  cl_assert_equal_i(43690,  f(third, 0, half));
  cl_assert_equal_i(87381,  f(two_third, 0, half));
  cl_assert_equal_i(174762, f(four_third, 0, half));

  cl_assert_equal_i(-131066, f(-half, half, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(-65533,  f(0, half, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(-21843,  f(third, half, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(21845,   f(two_third, half, ANIMATION_NORMALIZED_MAX));
  cl_assert_equal_i(109224,  f(four_third, half, ANIMATION_NORMALIZED_MAX));
}

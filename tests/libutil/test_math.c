/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/trig.h"
#include "util/math.h"

#include <math.h>
#include <inttypes.h>
#include <stdio.h>

#include <clar.h>

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_passert.h"

// Tests
///////////////////////////////////////////////////////////
static void check_atan2(int16_t x, int16_t y) {
  int32_t ours = atan2_lookup(y, x) * 180 / TRIG_PI;
  double theirs = atan2(y, x) / 3.14159 * 180;
  // atan2 returns in range [-pi, +pi], but we have [0,2pi].
  if (theirs < 0) theirs += 360;

  cl_assert(abs(ours - (int) theirs) < 3); // Allow 3 degrees difference max
}

static double log_two(uint32_t n) {
  return (log(n) / log(2));
}

static void check_ceil_log_two(uint32_t n) {
  int ours = ceil_log_two(n);
  int theirs = ceil(log_two(n));
  cl_assert(ours == theirs);
}

void test_math__initialize(void) {
}

void test_math__cleanup(void) {
}

void test_math__atan2(void) {
  check_atan2(10, 14);
  check_atan2(3, 5);
  check_atan2(5, 3);
  check_atan2(10, 10);
  check_atan2(-153, 217);
  check_atan2(-28, -133);
  check_atan2(323, -229);
  check_atan2(245, 196);
  check_atan2(65, -3);
  check_atan2(331, -320);
  check_atan2(-151, 284);
  check_atan2(111, -98);
  check_atan2(-44, -17);
  check_atan2(269, -356);
  check_atan2(-78, 268);
  check_atan2(-247, 37);
  check_atan2(-119, 33);
  check_atan2(234, -253);
  check_atan2(355, -193);
  check_atan2(-6, -310);
  check_atan2(15, -19);
  check_atan2(34, -32);
  check_atan2(-158, 299);
  check_atan2(120, 102);
  check_atan2(0, 0);
  check_atan2(0, 10);
  check_atan2(10, 0);
  check_atan2(-32768, 1); // <- causes overflow for int16
  check_atan2(1, -32768); // <- causes overflow for int16
  check_atan2(20001, 20000); // <- causes overflow if numbers are added in an int16
  check_atan2(32767, 1);
  check_atan2(1, 32767);
  check_atan2(32767, 0);
  check_atan2(0, 32767);
  check_atan2(23400, -25300);
  check_atan2(30500, -1930);
  check_atan2(-6, -310);
  check_atan2(15000, -19);
  check_atan2(34, -3200);
  check_atan2(-1508, 299);
  check_atan2(1020, 1002);
}

void test_math__ceil_log_two(void) {
  check_ceil_log_two(4);
  check_ceil_log_two(5);
  check_ceil_log_two(100);
  check_ceil_log_two(256);
  check_ceil_log_two(123456);
}

void test_math__sign_extend(void) {
  cl_assert_equal_i(sign_extend(0, 32), 0);
  cl_assert_equal_i(sign_extend(0, 3), 0);

  cl_assert_equal_i(sign_extend(1, 32), 1);
  cl_assert_equal_i(sign_extend(1, 3), 1);

  cl_assert_equal_i(sign_extend(-1, 32), -1);
  cl_assert_equal_i(sign_extend(-1, 3), -1);

  cl_assert_equal_i(sign_extend(7, 32), 7);
  cl_assert_equal_i(sign_extend(7, 3), -1);
}

void test_math__serial_distance32(void) {
  {
    int32_t dist = serial_distance32(0x0, 0x1);
    cl_assert_equal_i(dist,  1);
  }
  {
    int32_t dist = serial_distance32(0x1, 0x0);
    cl_assert_equal_i(dist,  -1);
  }
  {
    int32_t dist = serial_distance32(0x0, 0xffffffff);
    cl_assert_equal_i(dist,  -1);
  }
  {
    int32_t dist = serial_distance32(0xffffffff, 0x0);
    cl_assert_equal_i(dist,  1);
  }
  {
    int32_t dist = serial_distance32(0x0, 0x7fffffff);
    cl_assert_equal_i(dist,  0x7fffffff);
  }
}

void test_math__serial_distance3(void) {
  {
    int32_t dist = serial_distance(0, 1, 3);
    cl_assert_equal_i(dist, 1);
  }
  {
    int32_t dist = serial_distance(1, 0, 3);
    cl_assert_equal_i(dist, -1);
  }
  {
    int32_t dist = serial_distance(0, 7, 3);
    cl_assert_equal_i(dist, -1);
  }
  {
    int32_t dist = serial_distance(7, 0, 3);
    cl_assert_equal_i(dist, 1);
  }
  {
    int32_t dist = serial_distance(6, 0, 3);
    cl_assert_equal_i(dist, 2);
  }
  {
    int32_t dist = serial_distance(7, 1, 3);
    cl_assert_equal_i(dist, 2);
  }
  {
    int32_t dist = serial_distance(6, 1, 3);
    cl_assert_equal_i(dist, 3);
  }
}

void test_math__is_signed_macro(void) {
  cl_assert(IS_SIGNED((int)-1) == true);
  cl_assert(IS_SIGNED((unsigned int)1) == false);
}

void test_math__test_within_macro(void) {
  int16_t min;
  int16_t max;

  // Min and max are both positive
  ////////////////////////////////
  min = 5;
  max = 10;

  // Min and max themselves should satisfy WITHIN
  cl_assert_equal_b(WITHIN(min, min, max), true);
  cl_assert_equal_b(WITHIN(max, min, max), true);

  // In the middle of the bounds
  cl_assert_equal_b(WITHIN(7, min, max), true);

  // Just out of bounds
  cl_assert_equal_b(WITHIN(4, min, max), false);
  cl_assert_equal_b(WITHIN(11, min, max), false);

  // Negative out of bounds
  cl_assert_equal_i(WITHIN(-5, min, max), false);

  // Positive out of bounds
  cl_assert_equal_i(WITHIN(0, min, max), false);

  // Min negative, max positive
  ////////////////////////////////
  min = -10;
  max = 10;

  // Min and max themselves should satisfy WITHIN
  cl_assert_equal_b(WITHIN(min, min, max), true);
  cl_assert_equal_b(WITHIN(max, min, max), true);

  // In the middle of the bounds
  cl_assert_equal_i(WITHIN(-5, min, max), true);
  cl_assert_equal_i(WITHIN(0, min, max), true);
  cl_assert_equal_i(WITHIN(5, min, max), true);

  // Just out of bounds
  cl_assert_equal_i(WITHIN(-11, min, max), false);
  cl_assert_equal_i(WITHIN(11, min, max), false);

  // Min and max are both negative
  ////////////////////////////////
  min = -20;
  max = -10;

  // Min and max themselves should satisfy WITHIN
  cl_assert_equal_b(WITHIN(min, min, max), true);
  cl_assert_equal_b(WITHIN(max, min, max), true);

  // In the middle of the bounds
  cl_assert_equal_i(WITHIN(-15, min, max), true);

  // Just out of bounds
  cl_assert_equal_i(WITHIN(-21, min, max), false);
  cl_assert_equal_i(WITHIN(-9, min, max), false);

  // Positive out of bounds
  cl_assert_equal_i(WITHIN(0, min, max), false);
  cl_assert_equal_i(WITHIN(5, min, max), false);
}

void test_math__distance_to_boundary(void) {
  cl_assert_equal_i(10, distance_to_mod_boundary(10, 100));
  cl_assert_equal_i(50, distance_to_mod_boundary(50, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(90, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(110, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(210, 100));

  cl_assert_equal_i(10, distance_to_mod_boundary(-10, 100));
  cl_assert_equal_i(50, distance_to_mod_boundary(-50, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(-90, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(-110, 100));
  cl_assert_equal_i(10, distance_to_mod_boundary(-210, 100));
}

void test_math__gcd_zero(void) {
  cl_assert_equal_i(0, gcd(0, 0));
}

void test_math__gcd_coprime(void) {
  cl_assert_equal_i(1, gcd(8, 27));
}

void test_math__gcd_basic(void) {
  cl_assert_equal_i(9, gcd(9, 18));
}

void test_math__gcd_basic_reversed(void) {
  cl_assert_equal_i(9, gcd(18, 9));
}

void test_math__gcd_of_number_and_itself(void) {
  cl_assert_equal_i(10, gcd(10, 10));
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <math.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

// Correctly-rounded reference values, precomputed off-host so the expected
// results no longer depend on the host libm. See gen_log_reference.py.
#include "log_reference.h"

static double double_from_bits(uint64_t bits) {
  union { uint64_t bits; double value; } u = { .bits = bits };
  return u.value;
}

static int64_t ulp_diff(double a, double b) {
  union { double value; int64_t bits; } ua = { .value = a };
  union { double value; int64_t bits; } ub = { .value = b };
  int64_t diff = ua.bits - ub.bits;
  return diff < 0 ? -diff : diff;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_log__basic(void) {
  // log.c (newlib) documents its error as always less than 1 ulp relative to
  // the true value, so 1 ulp against the correctly-rounded reference is the
  // tolerance.
  for (int i = 0; i < LOG_REFERENCE_COUNT; i++) {
    double v = double_from_bits(s_log_reference[i].input_bits);
    double expected = double_from_bits(s_log_reference[i].expected_bits);
    double us = log(v);
    cl_assert(ulp_diff(us, expected) <= 1);
  }
  cl_assert(isnan(log(-1.0)));
  cl_assert(log(0) == -HUGE_VAL);
  cl_assert(isnan(log(NAN)));
  cl_assert(isinf(log(INFINITY)));
}

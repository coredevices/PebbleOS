/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <math.h>
#include <fenv.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

// Correctly-rounded reference values, precomputed off-host so the expected
// results no longer depend on the host libm. See gen_pow_reference.py.
#include "pow_reference.h"

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

void test_pow__initialize(void) {
  fesetround(FE_TONEAREST);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_pow__basic(void) {
  // pow.c (newlib) documents its result as "nearly rounded"; measured against
  // the correctly-rounded reference it is within 1 ulp, so 1 ulp is the
  // tolerance.
  for (int i = 0; i < POW_REFERENCE_COUNT; i++) {
    double v = double_from_bits(s_pow_reference[i].input_bits);
    double expected = double_from_bits(s_pow_reference[i].expected_bits);
    double us = pow(2, v);
    cl_assert(ulp_diff(us, expected) <= 1);
  }
}

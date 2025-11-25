/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <fenv.h>

#include "clar.h"

double log_theirs(double x) {
  return log(x);
}

// "Define" libc functions we're testing
#include "pblibc_private.h"

void test_pow__initialize(void) {
  fesetround(FE_TONEAREST);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_log__basic(void) {
  for(int i = 1; i < 10000; i++) {
    double v = i * 0.001;
    double us = log(v);
    double them = log_theirs(v);

    // 1 ulps is acceptable error
    // To actually check this, we need to do some sorta gross raw operations on the doubles
    int64_t diff = *(int64_t*)&us - *(int64_t*)&them;
    cl_assert(diff <= 1 && diff >= -1);
  }
  cl_assert(isnan(log(-1.0)));
  cl_assert(log(0) == -HUGE_VAL);
  cl_assert(isnan(log(NAN)));
  cl_assert(isinf(log(INFINITY)));
}

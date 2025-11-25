/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <math.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_round__round(void) {
  cl_assert_equal_d(round(0.0), 0.0);
  cl_assert_equal_d(round(0.4), 0.0);
  cl_assert_equal_d(round(-0.4), -0.0);
  cl_assert_equal_d(round(0.5), 1.0);
  cl_assert_equal_d(round(-0.5), -1.0);
  cl_assert_equal_d(round(-1.0), -1.0);
  cl_assert_equal_d(round(1.7976931348623157e+308), 1.7976931348623157e+308);
  cl_assert_equal_d(round(2.2250738585072014e-308), -0.0);
}

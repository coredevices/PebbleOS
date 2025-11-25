/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_floor__basic(void) {
  cl_assert(floor( 0.5) ==  0.0);
  cl_assert(floor(-0.5) == -1.0);
  cl_assert(floor( 0.0) ==  0.0);
  cl_assert(floor(-0.0) == -0.0);
  cl_assert(floor( 1.5) ==  1.0);
  cl_assert(floor(-1.5) == -2.0);
  cl_assert(floor( 2.0) ==  2.0);
  cl_assert(floor(-2.0) == -2.0);
  cl_assert(floor( 0.0001) ==  0.0);
  cl_assert(floor(-0.0001) == -1.0);
  cl_assert(isnan(floor(NAN)));
  cl_assert(isinf(floor(INFINITY)));
}

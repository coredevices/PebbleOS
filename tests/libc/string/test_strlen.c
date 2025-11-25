/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_strlen__basic(void) {
  char testbuf[9] = "hi";
  cl_assert_equal_i(strlen(testbuf), 2);
}

void test_strlen__weird(void) {
  char testbuf[9] = "hi\0five";
  cl_assert_equal_i(strlen(testbuf), 2);
}

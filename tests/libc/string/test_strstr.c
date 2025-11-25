/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_strstr__basic(void) {
  char testbuf[8] = "Hello!B";
  cl_assert_equal_p(strstr(testbuf, "lo!"), testbuf+3);
  cl_assert_equal_p(strstr(testbuf, "log"), NULL);
}

void test_strstr__weird(void) {
  char testbuf[8] = "He\0llo!B";
  cl_assert_equal_p(strstr(testbuf, "l"), NULL);
  cl_assert_equal_p(strstr(testbuf, "He"), testbuf);
}

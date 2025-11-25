/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_strspn__basic(void) {
  char testbuf[8] = "Hello!B";
  cl_assert_equal_i(strspn(testbuf, "Hel"), 4);
  cl_assert_equal_i(strspn(testbuf, "Heo"), 2);
  cl_assert_equal_i(strspn(testbuf, "B"), 0);
  cl_assert_equal_i(strspn(testbuf, "Helo!B"), 7);
}

void test_strspn__c_basic(void) {
  char testbuf[8] = "Hello!B";
  cl_assert_equal_i(strcspn(testbuf, "o!B"), 4);
  cl_assert_equal_i(strcspn(testbuf, "o"), 4);
  cl_assert_equal_i(strcspn(testbuf, "!"), 5);
  cl_assert_equal_i(strcspn(testbuf, "H"), 0);
}

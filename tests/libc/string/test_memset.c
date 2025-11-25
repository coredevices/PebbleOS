/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_memset__basic(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t expectbuf[8] = { 5, 5, 5, 5, 5, 5, 5, 5, };
  memset(testbuf, 5, 8);
  cl_assert_equal_m(testbuf, expectbuf, 8);
}

void test_memset__return(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  cl_assert_equal_p(memset(testbuf, 5, 8), testbuf);
}

void test_memset__partial(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t expectbuf[8] = { 0x12, 0x34, 0x56, 0x78, 5, 5, 5, 5, };
  memset(testbuf+4, 5, 4);
  cl_assert_equal_m(testbuf, expectbuf, 8);
}

void test_memset__big_value(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t expectbuf[8] = { 5, 5, 5, 5, 5, 5, 5, 5, };
  memset(testbuf, 0xF05, 8);
  cl_assert_equal_m(testbuf, expectbuf, 8);
}

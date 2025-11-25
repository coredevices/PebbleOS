/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_memcpy__basic(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  memcpy(destbuf, testbuf, 8);
  cl_assert_equal_m(testbuf, destbuf, 8);
}

void test_memcpy__partial(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8] = { 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, };
  uint8_t expectbuf[8] = { 0x9A, 0xBC, 0xDE, 0xF0, 0x78, 0x78, 0x78, 0x78, };
  memcpy(destbuf, testbuf+4, 4);
  cl_assert_equal_m(expectbuf, destbuf, 8);
}

void test_memcpy__return_value(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  cl_assert_equal_p(memcpy(destbuf, testbuf, 8), destbuf);
}

void test_memcpy__move_basic(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  memmove(destbuf, testbuf, 8);
  cl_assert_equal_m(testbuf, destbuf, 8);
}

void test_memcpy__move_return_value(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  cl_assert_equal_p(memmove(destbuf, testbuf, 8), destbuf);
}

void test_memcpy__move_overwrite_backwards(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  memcpy(destbuf+4, testbuf, 4);
  memmove(destbuf+2, destbuf+4, 4);
  cl_assert_equal_m(testbuf, destbuf+2, 4);
  cl_assert_equal_m(testbuf+2, destbuf+6, 2);
}

void test_memcpy__move_overwrite_forwards(void) {
  uint8_t testbuf[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, };
  uint8_t destbuf[8];
  memcpy(destbuf+0, testbuf, 4);
  memmove(destbuf+2, destbuf+0, 4);
  cl_assert_equal_m(testbuf, destbuf+2, 4);
  cl_assert_equal_m(testbuf, destbuf+0, 2);
}

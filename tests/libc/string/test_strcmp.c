/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests
// Asserting !strcmp as a bool:
//   True = equal
//   False = non-equal

void test_strcmp__same_buffer(void) {
  char testbuf[8] = "Hello!\0\0";
  cl_assert_equal_b(!strcmp(testbuf, testbuf), true);
}

void test_strcmp__same_content(void) {
  char testbuf1[8] = "Hello!\0\0";
  char testbuf2[8] = "Hello\0\0\0";
  testbuf2[5] = '!';
  cl_assert_equal_b(!strcmp(testbuf1, testbuf2), true);
}

void test_strcmp__different_content(void) {
  char testbuf1[8] = "Hello!\0\0";
  char testbuf2[8] = "Hello\0\0\0";
  cl_assert_equal_b(!strcmp(testbuf1, testbuf2), false);
}

void test_strcmp__n_same_buffer(void) {
  char testbuf[8] = "Hello!\0\0";
  cl_assert_equal_b(!strncmp(testbuf, testbuf, 8), true);
}

void test_strcmp__n_same_content(void) {
  char testbuf1[8] = "Hello!\0\0";
  char testbuf2[8] = "Hello\0\0\0";
  testbuf2[5] = '!';
  cl_assert_equal_b(!strncmp(testbuf1, testbuf2, 8), true);
}

void test_strcmp__n_different_content(void) {
  char testbuf1[8] = "Hello!\0\0";
  char testbuf2[8] = "Hello\0\0\0";
  cl_assert_equal_b(!strncmp(testbuf1, testbuf2, 8), false);
}

void test_strcmp__n_short(void) {
  char testbuf1[8] = "Hello!G\0";
  char testbuf2[8] = "HelloAB\0";
  cl_assert_equal_b(!strncmp(testbuf1, testbuf2, 5), true);
}

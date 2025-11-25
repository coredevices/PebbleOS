/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_strcat__basic(void) {
  char destbuf[9] = "hi";
  char expectbuf[9] = "hilarity";
  strcat(destbuf, "larity");
  cl_assert_equal_m(expectbuf, destbuf, 9);
}

void test_strcat__weird(void) {
  char destbuf[9] = "hi\0five";
  char expectbuf[9] = "hilarity";
  strcat(destbuf, "larity");
  cl_assert_equal_m(expectbuf, destbuf, 9);
}

void test_strcat__return(void) {
  char destbuf[9] = "hi";
  char expectbuf[9] = "hilarity";
  cl_assert_equal_p(strcat(destbuf, "larity"), destbuf);
}

void test_strcat__n_basic(void) {
  char destbuf[9] = "hi";
  char expectbuf[9] = "hilarity";
  strncat(destbuf, "larity", 6);
  cl_assert_equal_m(expectbuf, destbuf, 9);
}

void test_strcat__n_overlarge(void) {
  char destbuf[9] = "hi";
  char expectbuf[9] = "hilariou";
  strncat(destbuf, "lariousness", 6);
  cl_assert_equal_m(expectbuf, destbuf, 9);
}

void test_strcat__n_weird(void) {
  char destbuf[9] = "hi\0five";
  char expectbuf[9] = "hilariou";
  strncat(destbuf, "lariousness", 6);
  cl_assert_equal_m(expectbuf, destbuf, 9);
}

void test_strcat__n_return(void) {
  char destbuf[9] = "hi";
  char expectbuf[9] = "hilarity";
  cl_assert_equal_p(strncat(destbuf, "larity", 6), destbuf);
}

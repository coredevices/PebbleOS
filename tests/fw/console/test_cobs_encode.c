/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "clar.h"

#include "console/cobs.h"

unsigned char out[1024];

static void assert_out_not_touched(size_t index) {
  cl_assert_equal_i(out[index], 0xcc);
}

static void assert_out_equal(const void * restrict expected, size_t length) {
  cl_assert(memcmp(out, expected, length) == 0);
}

static void assert_encode(const void * restrict src, size_t in_length,
                          size_t expected_length) {
  size_t out_length = cobs_encode(out, src, in_length);
  cl_assert_equal_i(out_length, expected_length);
  if (expected_length != SIZE_MAX) {
    assert_out_not_touched(expected_length);
  }
}


void test_cobs_encode__initialize(void) {
  memset(out, 0xcc, sizeof(out));
}

void test_cobs_encode__empty(void) {
  assert_encode("", 0, 1);
  assert_out_equal("\x01", 1);
}

void test_cobs_encode__zero(void) {
  assert_encode("\0", 1, 2);
  assert_out_equal("\x01\x01", 2);
}

void test_cobs_encode__simple(void) {
  assert_encode("Hello", 5, 6);
  assert_out_equal("\x06Hello", 6);
}

void test_cobs_encode__multiple_blocks(void) {
  assert_encode("Hello\0w\0rld", 11, 12);
  assert_out_equal("\x06Hello\x02w\x04rld", 12);
}

void test_cobs_encode__max_block_1(void) {
  // vector[] = { 0x01, 0x02, ..., 0xfe };
  uint8_t vector[254];
  for (size_t i = 0; i < sizeof(vector); ++i) {
    vector[i] = i + 1;
  }
  // expected[] = { 0xff, 0x01, 0x02, ..., 0xfe };
  uint8_t expected[255];
  expected[0] = 0xff;
  for (size_t i = 1; i < sizeof(expected); ++i) {
    expected[i] = i;
  }
  assert_encode(vector, sizeof(vector), sizeof(expected));
  assert_out_equal(expected, sizeof(expected));
}
void test_cobs_encode__max_block_2(void) {
  // vector[] = { 0x01, 0x02, ..., 0xfe, 0xff };
  uint8_t vector[255];
  for (size_t i = 0; i < sizeof(vector); ++i) {
    vector[i] = i + 1;
  }
  // expected[] = { 0xff, 0x01, 0x02, ..., 0xfe, 0x02, 0xff };
  uint8_t expected[257];
  expected[0] = 0xff;
  uint8_t c = 1;
  for (int i = 1; i <= 255; ++i) {
    expected[i] = c++;
  }
  expected[255] = 0x02;
  expected[256] = 0xff;
  assert_encode(vector, sizeof(vector), sizeof(expected));
  assert_out_equal(expected, sizeof(expected));
}

void test_cobs_encode__max_block_3(void) {
  // vector[] = { 0x00, 0x01, 0x02, 0x03, ..., 0xfe, 0xff };
  uint8_t vector[256];
  for (size_t i = 0; i < sizeof(vector); ++i) {
    vector[i] = i;
  }
  // expected[] = { 0x01, 0xff, 0x01, 0x02, ..., 0xfe, 0x02, 0xff };
  uint8_t expected[258];
  expected[0] = 0x01;
  expected[1] = 0xff;
  uint8_t c = 1;
  for (int i = 2; i <= 256; ++i) {
    expected[i] = c++;
  }
  expected[256] = 0x02;
  expected[257] = 0xff;
  assert_encode(vector, sizeof(vector), sizeof(expected));
  assert_out_equal(expected, sizeof(expected));
}

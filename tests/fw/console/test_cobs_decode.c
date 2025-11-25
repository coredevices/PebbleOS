/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "clar.h"

#include "console/cobs.h"

static CobsDecodeContext ctx;
static unsigned char out[1024];

static void decode_start(size_t length) {
  cobs_streaming_decode_start(&ctx, out, length);
}

static void assert_decode_char_succeeds(char c) {
  cl_assert(cobs_streaming_decode(&ctx, c));
}

static void assert_decode_succeeds(const char * restrict buf, size_t length) {
  while (length--) {
    assert_decode_char_succeeds(*buf++);
  }
}

static void assert_decode_fails(const char * restrict buf, size_t length) {
  bool result = true;
  while (result && length--) {
    result = result && cobs_streaming_decode(&ctx, *buf++);
  }
  cl_assert(result == false);
}

static void assert_buffer_not_touched(size_t index) {
  cl_assert_equal_i(out[index], 0xcc);
}

static void assert_decode_completed(size_t expected_length) {
  size_t decoded_length = cobs_streaming_decode_finish(&ctx);
  cl_assert_equal_i(decoded_length, expected_length);
  if (decoded_length != SIZE_MAX) {
    assert_buffer_not_touched(decoded_length);
  }
}

static void assert_output_equal(const void * restrict expected, size_t length) {
  cl_assert(memcmp(out, expected, length) == 0);
}


void test_cobs_decode__initialize(void) {
  memset(&ctx, 0xdd, sizeof(ctx));
  memset(out, 0xcc, sizeof(out));
}

void test_cobs_decode__simple(void) {
  const char vector[] = { 0x06, 'H', 'e', 'l', 'l', 'o' };
  decode_start(5);
  assert_decode_succeeds(vector, sizeof(vector));
  assert_decode_completed(5);
  assert_output_equal("Hello", 5);
}

void test_cobs_decode__zeroes(void) {
  const char vector[] = { 0x01, 0x02, 'A', 0x01, 0x02, 'B', 0x01 };
  decode_start(6);
  assert_decode_succeeds(vector, sizeof(vector));
  assert_decode_completed(6);
  assert_output_equal("\0A\0\0B\0", 6);
}

void test_cobs_decode__max_length_block(void) {
  decode_start(254);
  assert_decode_char_succeeds(0xff);
  for (int c = 0x01; c <= 0xfe; ++c) {
    assert_decode_char_succeeds(c);
  }
  assert_decode_completed(254);
  const unsigned char *iter = out;
  for (int c = 0x01; c <= 0xfe; ++c) {
    cl_assert_equal_i(*iter++, c);
  }
}

void test_cobs_decode__empty_data(void) {
  decode_start(100);
  assert_decode_char_succeeds(0x01);
  assert_decode_completed(0);
  assert_buffer_not_touched(0);
}

void test_cobs_decode__output_too_small_1(void) {
  const char vector[] = { 0x06, 'L', 'a', 'r', 'g', 'e', '.' };
  decode_start(5);
  assert_decode_fails(vector, sizeof(vector));
  assert_decode_completed(SIZE_MAX);
  assert_buffer_not_touched(6);
}

void test_cobs_decode__output_too_small_2(void) {
  const char vector[] = { 0x05, 'a', 'b', 'c', 'd', 0x01, 0x01 };
  decode_start(5);
  assert_decode_fails(vector, sizeof(vector));
  assert_decode_completed(SIZE_MAX);
  assert_buffer_not_touched(6);
}

void test_cobs_decode__input_truncated(void) {
  const char vector[] = { 0x05, 'a', 'b', 'c' };
  decode_start(100);
  assert_decode_succeeds(vector, sizeof(vector));
  assert_decode_completed(SIZE_MAX);
}

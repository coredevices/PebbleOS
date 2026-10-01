/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/util/base64.h"

#include <string.h>

static void prv_check(const char *input, const char *expected) {
  char out[64];
  memset(out, 'x', sizeof(out));

  const size_t len = pbl_base64_encode(out, sizeof(out), input, strlen(input));
  cl_assert_equal_i(len, strlen(expected));
  cl_assert_equal_s(out, expected);
}

void test_base64__rfc4648_vectors(void) {
  prv_check("", "");
  prv_check("f", "Zg==");
  prv_check("fo", "Zm8=");
  prv_check("foo", "Zm9v");
  prv_check("foob", "Zm9vYg==");
  prv_check("fooba", "Zm9vYmE=");
  prv_check("foobar", "Zm9vYmFy");
}

void test_base64__full_alphabet(void) {
  const uint8_t data[] = {0x00, 0x10, 0x83, 0x10, 0x51, 0x87, 0x20, 0x92, 0x8b, 0x30, 0xd3, 0x8f,
                          0x41, 0x14, 0x93, 0x51, 0x55, 0x97, 0x61, 0x96, 0x9b, 0x71, 0xd7, 0x9f,
                          0x82, 0x18, 0xa3, 0x92, 0x59, 0xa7, 0xa2, 0x9a, 0xab, 0xb2, 0xdb, 0xaf,
                          0xc3, 0x1c, 0xb3, 0xd3, 0x5d, 0xb7, 0xe3, 0x9e, 0xbb, 0xf3, 0xdf, 0xbf};
  char out[65];
  cl_assert_equal_i(pbl_base64_encode(out, sizeof(out), data, sizeof(data)), 64);
  cl_assert_equal_s(out, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
}

void test_base64__too_small(void) {
  char out[4];
  memset(out, 'x', sizeof(out));
  cl_assert_equal_i(pbl_base64_encode(out, 3, "abc", 3), 4);
  cl_assert_equal_i(out[0], 'x');
}

void test_base64__no_room_for_terminator(void) {
  char out[5];
  memset(out, 'x', sizeof(out));
  cl_assert_equal_i(pbl_base64_encode(out, 4, "abc", 3), 4);
  cl_assert(memcmp(out, "YWJj", 4) == 0);
  cl_assert_equal_i(out[4], 'x');
}

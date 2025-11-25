/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/sle.h"

#include "stubs/stubs_passert.h"

void test_sle__simple(void) {
  SLEDecodeContext ctx;
  uint8_t buf[] = {
    0xfd, // escape code
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0xfd, 0x00 // end
  };
  sle_decode_init(&ctx, buf);

  uint8_t byte;
  uint8_t expect = 0x00;
  uint32_t count = 0;
  while (sle_decode(&ctx, &byte)) {
    cl_assert_equal_i(byte, expect++);
    ++count;
  }
  cl_assert_equal_i(count, 16);
}

void test_sle__short_zeros(void) {
  SLEDecodeContext ctx;
  uint8_t buf[] = {
    0xfd, // escape code
    0xfd, 0x5, // 5 zeroes
    0xfd, 0x00 // end
  };
  sle_decode_init(&ctx, buf);

  uint8_t byte;
  uint32_t count = 0;
  while (sle_decode(&ctx, &byte)) {
    cl_assert_equal_i(byte, 0x0);
    ++count;
  }
  cl_assert_equal_i(count, 5);
}

void test_sle__long_zeros(void) {
  SLEDecodeContext ctx;
  uint8_t buf[] = {
    0xfd, // escape code
    0xfd, 0xff, 0xaa, // 32810 zeroes
    0xfd, 0x00 // end
  };
  sle_decode_init(&ctx, buf);

  uint8_t byte;
  uint32_t count = 0;
  while (sle_decode(&ctx, &byte)) {
    cl_assert_equal_i(byte, 0x0);
    ++count;
  }
  cl_assert_equal_i(count, 32810);
}

void test_sle__escape_byte(void) {
  SLEDecodeContext ctx;
  uint8_t buf[] = {
    0xfd, // escape code
    0xfd, 0x01, // literal escape byte
    0xfd, 0x00 // end
  };
  sle_decode_init(&ctx, buf);

  uint8_t byte;
  uint32_t count = 0;
  while (sle_decode(&ctx, &byte)) {
    cl_assert_equal_i(byte, 0xfd);
    ++count;
  }
  cl_assert_equal_i(count, 1);
}

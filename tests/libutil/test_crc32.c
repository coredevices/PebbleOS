/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/crc32.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define assert_equal_hex(A, B) \
  do { \
    uint32_t a = (A); \
    uint32_t b = (B); \
    if (a != b) { \
      char error_msg[256]; \
      sprintf(error_msg, \
              "%#08"PRIx32" != %#08"PRIx32"\n", \
              a, b); \
      clar__assert(0, __FILE__, __LINE__, \
                   #A " != " #B, error_msg, 1); \
    } \
  } while (0)

static uint32_t crc;

void test_crc32__initialize(void) {
  crc = crc32(0, NULL, 0);
}

void test_crc32__initial_value_matches_header(void) {
  cl_assert_equal_i(crc, CRC32_INIT);
}

void test_crc32__null(void) {
  cl_assert_equal_i(crc, 0);
}

void test_crc32__empty_buffer(void) {
  crc = crc32(crc, "arbitrary pointer", 0);
  assert_equal_hex(crc, 0);
}

void test_crc32__one_byte(void) {
  crc = crc32(crc, "abcdefg", 1);
  assert_equal_hex(crc, 0xe8b7be43);
}

void test_crc32__standard_check(void) {
  // "Check" value from "A Painless Guide to CRC Error Detection Algorithms"
  crc = crc32(crc, "123456789", 9);
  assert_equal_hex(crc, 0xCBF43926);
}

void test_crc32__residue(void) {
  uint8_t message[30];
  memcpy(message, "1234567890", 10);
  crc = crc32(crc, message, 10);

  message[10] = crc & 0xff;
  message[11] = (crc >> 8) & 0xff;
  message[12] = (crc >> 16) & 0xff;
  message[13] = (crc >> 24) & 0xff;
  assert_equal_hex(crc32(crc32(0, NULL, 0), message, 14), CRC32_RESIDUE);
}

void test_crc32__null_residue(void) {
  uint8_t data[4] = {0, 0, 0, 0};
  crc = crc32(crc, data, 4);
  assert_equal_hex(crc, CRC32_RESIDUE);
}

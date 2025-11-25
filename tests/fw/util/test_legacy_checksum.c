/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/legacy_checksum.h"

#include <inttypes.h>
#include <stdio.h>

LegacyChecksum cksum;

void test_legacy_checksum__initialize(void) {
  memset(&cksum, 0xcc, sizeof cksum);
  legacy_defective_checksum_init(&cksum);
}

static void update(const void * restrict data, size_t length) {
  legacy_defective_checksum_update(&cksum, data, length);
}

#define assert_checksum(EXPECTED) \
  do { \
    uint32_t checksum = legacy_defective_checksum_finish(&cksum); \
    uint32_t expected = (EXPECTED); \
    if (checksum != expected) { \
      char error_msg[256]; \
      sprintf(error_msg, \
              "%#08"PRIx32" != %#08"PRIx32"\n", \
              expected, checksum); \
      clar__assert(0, __FILE__, __LINE__, \
                   "  expected != checksum", error_msg, 1); \
    } \
  } while (0)



void test_legacy_checksum__no_data(void) {
  assert_checksum(0xffffffff);
}

void test_legacy_checksum__one_byte(void) {
  update("A", 1);
  assert_checksum(0xf743b0bb);
}

void test_legacy_checksum__standard(void) {
  update("123456789", 9);
  assert_checksum(0xaff19057);
}

void test_legacy_checksum__one_word(void) {
  update("1234", 4);
  assert_checksum(0xc2091428);
}

void test_legacy_checksum__repeated_byte(void) {
  update("1111", 4);
  assert_checksum(0x13cbc447);
}

void test_legacy_checksum__two_words(void) {
  update("abcd", 4);
  update("efgh", 4);
  assert_checksum(0x18c4859c);
}

void test_legacy_checksum__split_word(void) {
  update("123", 3);
  update("4", 1);
  assert_checksum(0xc2091428);
}

void test_legacy_checksum__finish_with_partial(void) {
  update("1234", 4);
  update("5", 1);
  assert_checksum(0xec5baa37);
}

void test_legacy_checksum__start_with_partial(void) {
  update("123", 3);
  update("4567", 4);
  update("8", 1);
  assert_checksum(0xfefc54f9);
}

void test_legacy_checksum__start_and_finish_with_partial(void) {
  update("12", 2);
  update("3456", 4);
  update("78", 2);
  assert_checksum(0xfefc54f9);
}

void test_legacy_checksum__long_input(void) {
  update("1234567890abcdefghijklmnopqrstuvwxyz", 36);
  assert_checksum(0x586c447d);
}

void test_legacy_checksum__convenience_wrapper(void) {
  uint32_t sum = legacy_defective_checksum_memory("12345", 5);
  cl_assert_equal_i(sum, 0xec5baa37);
}

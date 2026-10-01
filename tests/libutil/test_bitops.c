/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/util/bitops.h"

void test_bitops__bitset8(void) {
  uint8_t set[3] = {0};

  pbl_bitset8_set(set, 0);
  pbl_bitset8_set(set, 9);
  pbl_bitset8_set(set, 23);
  cl_assert_equal_i(set[0], 0x01);
  cl_assert_equal_i(set[1], 0x02);
  cl_assert_equal_i(set[2], 0x80);
  cl_assert(pbl_bitset8_get(set, 9));
  cl_assert(!pbl_bitset8_get(set, 10));

  pbl_bitset8_clear(set, 9);
  cl_assert_equal_i(set[1], 0x00);

  pbl_bitset8_update(set, 12, true);
  cl_assert(pbl_bitset8_get(set, 12));
  pbl_bitset8_update(set, 12, false);
  cl_assert(!pbl_bitset8_get(set, 12));
  cl_assert_equal_i(set[0], 0x01);
  cl_assert_equal_i(set[2], 0x80);
}

void test_bitops__bitset32(void) {
  uint32_t set[2] = {0};

  pbl_bitset32_set(set, 31);
  pbl_bitset32_set(set, 32);
  cl_assert_equal_i(set[0], 0x80000000);
  cl_assert_equal_i(set[1], 0x00000001);
  cl_assert(pbl_bitset32_get(set, 31));
  cl_assert(pbl_bitset32_get(set, 32));
  cl_assert(!pbl_bitset32_get(set, 0));

  pbl_bitset32_clear(set, 31);
  cl_assert_equal_i(set[0], 0);

  pbl_bitset32_update(set, 5, true);
  cl_assert_equal_i(set[0], 0x20);
  pbl_bitset32_update(set, 5, false);
  cl_assert_equal_i(set[0], 0);
}

void test_bitops__rotl32(void) {
  cl_assert_equal_i(pbl_rotl32(0x80000001, 0), 0x80000001);
  cl_assert_equal_i(pbl_rotl32(0x80000001, 1), 0x00000003);
  cl_assert_equal_i(pbl_rotl32(0x12345678, 8), 0x34567812);
  cl_assert_equal_i(pbl_rotl32(0x12345678, 32), 0x12345678);
  cl_assert_equal_i(pbl_rotl32(0x12345678, 40), 0x34567812);
  cl_assert_equal_i(pbl_rotl32(0x12345678, (unsigned int)-8), 0x78123456);
}

void test_bitops__bitrev8(void) {
  cl_assert_equal_i(pbl_bitrev8(0x00), 0x00);
  cl_assert_equal_i(pbl_bitrev8(0x01), 0x80);
  cl_assert_equal_i(pbl_bitrev8(0x0f), 0xf0);
  cl_assert_equal_i(pbl_bitrev8(0xa5), 0xa5);
  cl_assert_equal_i(pbl_bitrev8(0x12), 0x48);
  for (unsigned int v = 0; v < 256; v++) {
    cl_assert_equal_i(pbl_bitrev8(pbl_bitrev8((uint8_t)v)), v);
  }
}

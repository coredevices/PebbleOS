/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/util/bits.h>

#include <clar.h>

_Static_assert(PBL_GENMASK(7, 4) == 0xF0U, "");
_Static_assert(PBL_FIELD_PREP(PBL_GENMASK(7, 4), 0x4U) == 0x40U, "");

#if PBL_GENMASK(3, 0) != 0x0FU
#error "PBL_GENMASK not usable in #if"
#endif

void test_bits__bit(void) {
  cl_assert_equal_i(PBL_BIT(0), 0x1);
  cl_assert_equal_i(PBL_BIT(7), 0x80);
  cl_assert(PBL_BIT(31) == 0x80000000U);
  cl_assert(PBL_BIT64(63) == 0x8000000000000000ULL);
  cl_assert(sizeof(PBL_BIT(0)) == sizeof(uint32_t));
  cl_assert(sizeof(PBL_BIT64(0)) == sizeof(uint64_t));
}

void test_bits__bit_mask(void) {
  cl_assert_equal_i(PBL_BIT_MASK(0), 0x0);
  cl_assert_equal_i(PBL_BIT_MASK(1), 0x1);
  cl_assert_equal_i(PBL_BIT_MASK(12), 0xFFF);
  cl_assert(PBL_BIT_MASK(31) == 0x7FFFFFFFU);
  cl_assert(PBL_BIT64_MASK(40) == 0xFFFFFFFFFFULL);
}

void test_bits__genmask(void) {
  cl_assert_equal_i(PBL_GENMASK(0, 0), 0x1);
  cl_assert_equal_i(PBL_GENMASK(7, 0), 0xFF);
  cl_assert_equal_i(PBL_GENMASK(6, 5), 0x60);
  cl_assert(PBL_GENMASK(31, 0) == 0xFFFFFFFFU);
  cl_assert(PBL_GENMASK(31, 31) == 0x80000000U);
  cl_assert(sizeof(PBL_GENMASK(31, 0)) == sizeof(uint32_t));
  cl_assert(PBL_GENMASK64(63, 0) == 0xFFFFFFFFFFFFFFFFULL);
  cl_assert(PBL_GENMASK64(39, 32) == 0xFF00000000ULL);
}

void test_bits__lsb_get(void) {
  cl_assert_equal_i(PBL_LSB_GET(0U), 0);
  cl_assert_equal_i(PBL_LSB_GET(0x0CU), 0x04);
  cl_assert(PBL_LSB_GET(0x80000000U) == 0x80000000U);
}

void test_bits__field_get(void) {
  uint8_t reg = 0xB6;

  cl_assert_equal_i(PBL_FIELD_GET(PBL_GENMASK(7, 4), reg), 0xB);
  cl_assert_equal_i(PBL_FIELD_GET(PBL_GENMASK(3, 2), reg), 0x1);
  cl_assert_equal_i(PBL_FIELD_GET(PBL_BIT(0), reg), 0);
  cl_assert_equal_i(PBL_FIELD_GET(PBL_BIT(1), reg), 1);
  cl_assert(PBL_FIELD_GET(PBL_GENMASK(31, 28), 0xA0000000U) == 0xAU);
  cl_assert(PBL_FIELD_GET(PBL_GENMASK64(47, 40), 0x123456789ABCULL) == 0x12ULL);
}

void test_bits__field_prep(void) {
  cl_assert_equal_i(PBL_FIELD_PREP(PBL_GENMASK(7, 4), 0x9), 0x90);
  cl_assert_equal_i(PBL_FIELD_PREP(PBL_GENMASK(6, 5), 0x7), 0x60);
  cl_assert_equal_i(PBL_FIELD_PREP(PBL_GENMASK(4, 0), 0), 0);
  cl_assert(PBL_FIELD_PREP(PBL_GENMASK(31, 30), 0x3U) == 0xC0000000U);
  cl_assert(PBL_FIELD_PREP(PBL_GENMASK64(47, 40), 0x12ULL) == 0x120000000000ULL);

  for (unsigned int v = 0; v < 16; v++) {
    cl_assert_equal_i(PBL_FIELD_GET(PBL_GENMASK(5, 2), PBL_FIELD_PREP(PBL_GENMASK(5, 2), v)), v);
  }
}

void test_bits__write_bit(void) {
  uint32_t var = 0x10;

  PBL_WRITE_BIT(var, 0, true);
  cl_assert_equal_i(var, 0x11);
  PBL_WRITE_BIT(var, 4, false);
  cl_assert_equal_i(var, 0x01);
  PBL_WRITE_BIT(var, 31, 1);
  cl_assert(var == 0x80000001U);
  PBL_WRITE_BIT(var, 31, 0);
  cl_assert_equal_i(var, 0x01);

  uint64_t var64 = 0x100000001ULL;

  PBL_WRITE_BIT(var64, 0, false);
  cl_assert(var64 == 0x100000000ULL);
  PBL_WRITE_BIT(var64, 63, true);
  cl_assert(var64 == 0x8000000100000000ULL);
  PBL_WRITE_BIT(var64, 32, false);
  cl_assert(var64 == 0x8000000000000000ULL);

  uint8_t var8 = 0xFF;

  PBL_WRITE_BIT(var8, 3, false);
  cl_assert_equal_i(var8, 0xF7);
  PBL_WRITE_BIT(var8, 3, true);
  cl_assert_equal_i(var8, 0xFF);
}

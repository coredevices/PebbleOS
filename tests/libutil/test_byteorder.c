/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/util/byteorder.h"

#include <string.h>

void test_byteorder__be16(void) {
  const uint8_t wire[] = {0x12, 0x34};
  uint16_t raw;
  memcpy(&raw, wire, sizeof(raw));

  cl_assert_equal_i(pbl_be16_to_cpu(raw), 0x1234);
  cl_assert_equal_i(pbl_cpu_to_be16(0x1234), raw);
  cl_assert_equal_i(pbl_be16_to_cpu(pbl_cpu_to_be16(0xbeef)), 0xbeef);
}

void test_byteorder__be32(void) {
  const uint8_t wire[] = {0x12, 0x34, 0x56, 0x78};
  uint32_t raw;
  memcpy(&raw, wire, sizeof(raw));

  cl_assert_equal_i(pbl_be32_to_cpu(raw), 0x12345678);
  cl_assert_equal_i(pbl_cpu_to_be32(0x12345678), raw);
  cl_assert_equal_i(pbl_be32_to_cpu(pbl_cpu_to_be32(0xdeadbeef)), 0xdeadbeef);
}

void test_byteorder__le(void) {
  const uint8_t wire[] = {0x78, 0x56, 0x34, 0x12};
  uint16_t raw16;
  uint32_t raw32;
  memcpy(&raw16, wire, sizeof(raw16));
  memcpy(&raw32, wire, sizeof(raw32));

  cl_assert_equal_i(pbl_le16_to_cpu(raw16), 0x5678);
  cl_assert_equal_i(pbl_le32_to_cpu(raw32), 0x12345678);
  cl_assert_equal_i(pbl_cpu_to_le16(0x5678), raw16);
  cl_assert_equal_i(pbl_cpu_to_le32(0x12345678), raw32);
}

void test_byteorder__be16_wrapper(void) {
  const pbl_be16_t be = pbl_be16_make(0xabcd);
  const uint8_t *bytes = (const uint8_t *)&be;

  cl_assert_equal_i(bytes[0], 0xab);
  cl_assert_equal_i(bytes[1], 0xcd);
  cl_assert_equal_i(pbl_be16_get(be), 0xabcd);
}

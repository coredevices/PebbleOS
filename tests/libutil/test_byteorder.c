/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/util/byteorder.h>

#include <clar.h>

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

void test_byteorder__be64(void) {
  const uint8_t wire[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  uint64_t raw;
  memcpy(&raw, wire, sizeof(raw));

  cl_assert(pbl_be64_to_cpu(raw) == 0x0123456789abcdefULL);
  cl_assert(pbl_cpu_to_be64(0x0123456789abcdefULL) == raw);
  cl_assert(pbl_le64_to_cpu(raw) == 0xefcdab8967452301ULL);
  cl_assert(pbl_cpu_to_le64(0xefcdab8967452301ULL) == raw);
}

void test_byteorder__get_unaligned(void) {
  const uint8_t buf[] = {0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  const uint8_t *p = &buf[1];

  cl_assert_equal_i(pbl_get_le16(p), 0x0201);
  cl_assert_equal_i(pbl_get_be16(p), 0x0102);
  cl_assert_equal_i(pbl_get_le24(p), 0x030201);
  cl_assert_equal_i(pbl_get_be24(p), 0x010203);
  cl_assert_equal_i(pbl_get_le32(p), 0x04030201);
  cl_assert_equal_i(pbl_get_be32(p), 0x01020304);
  cl_assert(pbl_get_le64(p) == 0x0807060504030201ULL);
  cl_assert(pbl_get_be64(p) == 0x0102030405060708ULL);
}

void test_byteorder__put_unaligned(void) {
  const uint8_t le[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  const uint8_t be[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
  uint8_t buf[10];

  memset(buf, 0xaa, sizeof(buf));
  pbl_put_le16(0x0201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], le, 2), 0);
  pbl_put_be16(0x0201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], &be[6], 2), 0);

  pbl_put_le24(0x030201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], le, 3), 0);
  pbl_put_be24(0x030201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], &be[5], 3), 0);

  pbl_put_le32(0x04030201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], le, 4), 0);
  pbl_put_be32(0x04030201, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], &be[4], 4), 0);

  pbl_put_le64(0x0807060504030201ULL, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], le, 8), 0);
  pbl_put_be64(0x0807060504030201ULL, &buf[1]);
  cl_assert_equal_i(memcmp(&buf[1], be, 8), 0);

  cl_assert_equal_i(buf[0], 0xaa);
  cl_assert_equal_i(buf[9], 0xaa);
}

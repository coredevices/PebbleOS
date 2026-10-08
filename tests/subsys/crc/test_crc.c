/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "crc_reference.h"

#include <string.h>

#include <pbl/crc/crc.h>

#include <clar.h>

static const char s_check[] = "123456789";
static uint8_t s_data[1031];

void test_crc__initialize(void) {
  uint32_t x = 0x12345678;
  for (size_t i = 0; i < sizeof(s_data); i++) {
    x = x * 1103515245U + 12345U;
    s_data[i] = (uint8_t)(x >> 16);
  }
}

void test_crc__crc8_check(void) {
  cl_assert_equal_i(pbl_crc8(0, s_check, strlen(s_check)), 0x3e);
  cl_assert_equal_i(pbl_crc8_reversed(0, s_check, strlen(s_check)), 0x9b);
  cl_assert_equal_i(pbl_crc8(0, s_check, 0), 0);
}

void test_crc__crc8_streaming(void) {
  const uint8_t whole = pbl_crc8(0, s_data, sizeof(s_data));
  for (size_t split = 0; split <= 16; split++) {
    cl_assert_equal_i(pbl_crc8(pbl_crc8(0, s_data, split), &s_data[split], sizeof(s_data) - split),
                      whole);
  }
}

void test_crc__crc8_reversed_is_crc8_of_reversed_bytes(void) {
  uint8_t reversed[64];
  for (size_t i = 0; i < sizeof(reversed); i++) {
    reversed[i] = s_data[sizeof(reversed) - 1 - i];
  }
  cl_assert_equal_i(pbl_crc8_reversed(0, s_data, sizeof(reversed)),
                    pbl_crc8(0, reversed, sizeof(reversed)));
}

void test_crc__crc32_check(void) {
  cl_assert_equal_i(pbl_crc32(0, s_check, strlen(s_check)), 0xcbf43926);
  cl_assert_equal_i(pbl_crc32(0, NULL, 0), 0);
  cl_assert_equal_i(pbl_crc32(0, s_check, 0), 0);
  cl_assert_equal_i(pbl_crc32(0, s_data, sizeof(s_data)), ref_crc32(0, s_data, sizeof(s_data)));
}

void test_crc__crc32_streaming(void) {
  const uint32_t whole = pbl_crc32(0, s_data, sizeof(s_data));
  for (size_t split = 0; split < sizeof(s_data); split += 97) {
    const uint32_t head = pbl_crc32(0, s_data, split);
    cl_assert_equal_i(pbl_crc32(head, &s_data[split], sizeof(s_data) - split), whole);
  }
}

void test_crc__crc32_residue(void) {
  uint8_t msg[32];
  memcpy(msg, s_data, 28);
  const uint32_t crc = pbl_crc32(0, msg, 28);
  memcpy(&msg[28], &crc, sizeof(crc));
  cl_assert_equal_i(pbl_crc32(0, msg, sizeof(msg)), PBL_CRC32_RESIDUE);
}

void test_crc__legacy_vectors(void) {
  cl_assert_equal_i(pbl_crc32_legacy(s_check, strlen(s_check)), 0xaff19057);
  cl_assert_equal_i(pbl_crc32_legacy("1234", 4), 0xc2091428);
  cl_assert_equal_i(pbl_crc32_legacy(s_check, 0), 0xffffffff);
  for (size_t len = 0; len < 64; len++) {
    cl_assert_equal_i(pbl_crc32_legacy(s_data, len), ref_legacy(s_data, len));
  }
}

void test_crc__legacy_streaming(void) {
  const uint32_t whole = pbl_crc32_legacy(s_data, sizeof(s_data));
  for (size_t first = 0; first < 9; first++) {
    for (size_t second = 0; second < 9; second++) {
      struct pbl_crc32_legacy ctx;
      pbl_crc32_legacy_init(&ctx);
      pbl_crc32_legacy_update(&ctx, s_data, first);
      pbl_crc32_legacy_update(&ctx, &s_data[first], second);
      pbl_crc32_legacy_update(&ctx, &s_data[first + second], sizeof(s_data) - first - second);
      cl_assert_equal_i(pbl_crc32_legacy_finish(&ctx), whole);
    }
  }
}

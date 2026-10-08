/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "crc_reference.h"

#include <pbl/crc/crc.h>
#include <pbl/drivers/crc.h>

#include <clar.h>

static uint8_t s_data[1031];
static bool s_hw_available;
static unsigned int s_hw_calls;
static size_t s_hw_bytes;

size_t pbl_crc_hw_crc32(uint32_t *crc, const void *data, size_t len) {
  s_hw_calls++;
  if (!s_hw_available) {
    return 0;
  }
  const size_t done = (len / 2) & ~(size_t)3;
  *crc = ref_crc32(*crc, data, done);
  s_hw_bytes += done;
  return done;
}

size_t pbl_crc_hw_crc32_legacy(uint32_t *reg, const void *data, size_t len) {
  s_hw_calls++;
  if (!s_hw_available) {
    return 0;
  }
  const size_t done = (len / 2) & ~(size_t)3;
  *reg = ref_legacy_words(*reg, data, done);
  s_hw_bytes += done;
  return done;
}

void test_crc_hw__initialize(void) {
  uint32_t x = 0xcafef00d;
  for (size_t i = 0; i < sizeof(s_data); i++) {
    x = x * 1103515245U + 12345U;
    s_data[i] = (uint8_t)(x >> 16);
  }
  s_hw_available = true;
  s_hw_calls = 0;
  s_hw_bytes = 0;
}

void test_crc_hw__crc32_partial_offload(void) {
  cl_assert_equal_i(pbl_crc32(0, s_data, sizeof(s_data)), ref_crc32(0, s_data, sizeof(s_data)));
  cl_assert_equal_i(s_hw_calls, 1);
  cl_assert(s_hw_bytes > 0);

  const uint32_t head = pbl_crc32(0, s_data, 100);
  cl_assert_equal_i(pbl_crc32(head, &s_data[100], sizeof(s_data) - 100),
                    ref_crc32(0, s_data, sizeof(s_data)));
}

void test_crc_hw__crc32_short_stays_in_software(void) {
  cl_assert_equal_i(pbl_crc32(0, s_data, 15), ref_crc32(0, s_data, 15));
  cl_assert_equal_i(s_hw_calls, 0);
}

void test_crc_hw__crc32_unavailable_falls_back(void) {
  s_hw_available = false;
  cl_assert_equal_i(pbl_crc32(0, s_data, sizeof(s_data)), ref_crc32(0, s_data, sizeof(s_data)));
  cl_assert_equal_i(s_hw_calls, 1);
}

void test_crc_hw__legacy_partial_offload(void) {
  for (size_t first = 0; first < 8; first++) {
    struct pbl_crc32_legacy ctx;
    pbl_crc32_legacy_init(&ctx);
    pbl_crc32_legacy_update(&ctx, s_data, first);
    pbl_crc32_legacy_update(&ctx, &s_data[first], sizeof(s_data) - first);
    cl_assert_equal_i(pbl_crc32_legacy_finish(&ctx), ref_legacy(s_data, sizeof(s_data)));
  }
  cl_assert(s_hw_bytes > 0);
}

void test_crc_hw__legacy_unavailable_falls_back(void) {
  s_hw_available = false;
  cl_assert_equal_i(pbl_crc32_legacy(s_data, sizeof(s_data)), ref_legacy(s_data, sizeof(s_data)));
  cl_assert(s_hw_calls > 0);
}

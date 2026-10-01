/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

static inline uint32_t ref_crc32(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
    }
  }
  return ~crc;
}

static inline uint32_t ref_mpeg2_byte(uint32_t reg, uint8_t byte) {
  reg ^= (uint32_t)byte << 24;
  for (int bit = 0; bit < 8; bit++) {
    reg = (reg << 1) ^ ((reg & 0x80000000U) ? 0x04c11db7U : 0);
  }
  return reg;
}

static inline uint32_t ref_legacy_words(uint32_t reg, const uint8_t *data, size_t len) {
  for (size_t i = 0; i + 4 <= len; i += 4) {
    for (int b = 3; b >= 0; b--) {
      reg = ref_mpeg2_byte(reg, data[i + b]);
    }
  }
  return reg;
}

static inline uint32_t ref_legacy(const uint8_t *data, size_t len) {
  const size_t words = len & ~(size_t)3;
  uint32_t reg = ref_legacy_words(0xffffffffU, data, words);
  if (len > words) {
    for (size_t i = len - words; i < 4; i++) {
      reg = ref_mpeg2_byte(reg, 0);
    }
    for (size_t i = words; i < len; i++) {
      reg = ref_mpeg2_byte(reg, data[i]);
    }
  }
  return reg;
}

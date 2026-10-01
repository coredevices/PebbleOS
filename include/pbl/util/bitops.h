/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline void pbl_bitset8_set(uint8_t *bitset, unsigned int index) {
  bitset[index / 8] |= (uint8_t)(1U << (index % 8));
}

static inline void pbl_bitset8_clear(uint8_t *bitset, unsigned int index) {
  bitset[index / 8] &= (uint8_t)~(1U << (index % 8));
}

static inline void pbl_bitset8_update(uint8_t *bitset, unsigned int index, bool value) {
  if (value) {
    pbl_bitset8_set(bitset, index);
  } else {
    pbl_bitset8_clear(bitset, index);
  }
}

static inline bool pbl_bitset8_get(const uint8_t *bitset, unsigned int index) {
  return bitset[index / 8] & (1U << (index % 8));
}

static inline void pbl_bitset32_set(uint32_t *bitset, unsigned int index) {
  bitset[index / 32] |= (1UL << (index % 32));
}

static inline void pbl_bitset32_clear(uint32_t *bitset, unsigned int index) {
  bitset[index / 32] &= ~(1UL << (index % 32));
}

static inline void pbl_bitset32_update(uint32_t *bitset, unsigned int index, bool value) {
  if (value) {
    pbl_bitset32_set(bitset, index);
  } else {
    pbl_bitset32_clear(bitset, index);
  }
}

static inline bool pbl_bitset32_get(const uint32_t *bitset, unsigned int index) {
  return bitset[index / 32] & (1UL << (index % 32));
}

static inline uint32_t pbl_rotl32(uint32_t x, unsigned int shift) {
  shift &= 31U;
  return (x << shift) | (x >> (-shift & 31U));
}

static inline uint8_t pbl_bitrev8(uint8_t v) {
#if defined(__thumb2__)
  uint32_t r;
  __asm__("rbit %0, %1" : "=r"(r) : "r"((uint32_t)v));
  return (uint8_t)(r >> 24);
#else
  v = (uint8_t)(((v & 0xF0U) >> 4) | ((v & 0x0FU) << 4));
  v = (uint8_t)(((v & 0xCCU) >> 2) | ((v & 0x33U) << 2));
  return (uint8_t)(((v & 0xAAU) >> 1) | ((v & 0x55U) << 1));
#endif
}

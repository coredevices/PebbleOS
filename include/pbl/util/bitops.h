/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup util_bitops Bit operations
 * @ingroup util
 * @brief Bit sets stored in byte or word arrays, rotation and bit reversal.
 *
 * Bit @c index of a bit set lives in element <tt>index / width</tt>, at bit
 * <tt>index % width</tt>. No bounds checking is done.
 *
 * @code{.c}
 * uint32_t active[2] = {0};
 *
 * pbl_bitset32_set(active, 40);
 * if (pbl_bitset32_get(active, 40)) {
 *   ...
 * }
 * @endcode
 * @{
 */

/**
 * @brief Set a bit in a byte-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 */
static inline void pbl_bitset8_set(uint8_t *bitset, unsigned int index) {
  bitset[index / 8] |= (uint8_t)(1U << (index % 8));
}

/**
 * @brief Clear a bit in a byte-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 */
static inline void pbl_bitset8_clear(uint8_t *bitset, unsigned int index) {
  bitset[index / 8] &= (uint8_t)~(1U << (index % 8));
}

/**
 * @brief Set or clear a bit in a byte-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 * @param value New value of the bit.
 */
static inline void pbl_bitset8_update(uint8_t *bitset, unsigned int index, bool value) {
  if (value) {
    pbl_bitset8_set(bitset, index);
  } else {
    pbl_bitset8_clear(bitset, index);
  }
}

/**
 * @brief Test a bit in a byte-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 * @return Value of the bit.
 */
static inline bool pbl_bitset8_get(const uint8_t *bitset, unsigned int index) {
  return bitset[index / 8] & (1U << (index % 8));
}

/**
 * @brief Set a bit in a word-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 */
static inline void pbl_bitset32_set(uint32_t *bitset, unsigned int index) {
  bitset[index / 32] |= (1UL << (index % 32));
}

/**
 * @brief Clear a bit in a word-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 */
static inline void pbl_bitset32_clear(uint32_t *bitset, unsigned int index) {
  bitset[index / 32] &= ~(1UL << (index % 32));
}

/**
 * @brief Set or clear a bit in a word-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 * @param value New value of the bit.
 */
static inline void pbl_bitset32_update(uint32_t *bitset, unsigned int index, bool value) {
  if (value) {
    pbl_bitset32_set(bitset, index);
  } else {
    pbl_bitset32_clear(bitset, index);
  }
}

/**
 * @brief Test a bit in a word-array bit set.
 *
 * @param bitset Bit set.
 * @param index Bit index.
 * @return Value of the bit.
 */
static inline bool pbl_bitset32_get(const uint32_t *bitset, unsigned int index) {
  return bitset[index / 32] & (1UL << (index % 32));
}

/**
 * @brief Rotate a 32-bit value left.
 *
 * @param x Value to rotate.
 * @param shift Number of bits, taken modulo 32.
 * @return Rotated value.
 */
static inline uint32_t pbl_rotl32(uint32_t x, unsigned int shift) {
  shift &= 31U;
  return (x << shift) | (x >> (-shift & 31U));
}

/**
 * @brief Reverse the bit order of a byte.
 *
 * @param v Byte to reverse.
 * @return @p v with bit 0 swapped with bit 7, bit 1 with bit 6, and so on.
 */
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

/** @} */

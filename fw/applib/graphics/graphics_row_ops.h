/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Fills a row of an 8-bit bitmap a word at a time; libc's memset goes byte by byte.
static inline void graphics_row_fill(uint8_t *dst, uint8_t value, size_t n) {
  while (n > 0U && ((uintptr_t)dst & 3U) != 0U) {
    *dst++ = value;
    n--;
  }
  const uint32_t word = value * 0x01010101U;
  uint32_t *dst_word = (uint32_t *)dst;
  for (; n >= 16U; n -= 16U, dst_word += 4) {
    dst_word[0] = word;
    dst_word[1] = word;
    dst_word[2] = word;
    dst_word[3] = word;
  }
  dst = (uint8_t *)dst_word;
  while (n-- > 0U) {
    *dst++ = value;
  }
}

//! Copies a row of an 8-bit bitmap a word at a time when both ends align. Must not overlap.
static inline void graphics_row_copy(uint8_t *restrict dst, const uint8_t *restrict src, size_t n) {
  if ((((uintptr_t)dst ^ (uintptr_t)src) & 3U) == 0U) {
    while (n > 0U && ((uintptr_t)dst & 3U) != 0U) {
      *dst++ = *src++;
      n--;
    }
    uint32_t *restrict dst_word = (uint32_t *)dst;
    const uint32_t *restrict src_word = (const uint32_t *)src;
    for (; n >= 16U; n -= 16U, dst_word += 4, src_word += 4) {
      dst_word[0] = src_word[0];
      dst_word[1] = src_word[1];
      dst_word[2] = src_word[2];
      dst_word[3] = src_word[3];
    }
    dst = (uint8_t *)dst_word;
    src = (const uint8_t *)src_word;
  }
  while (n-- > 0U) {
    *dst++ = *src++;
  }
}

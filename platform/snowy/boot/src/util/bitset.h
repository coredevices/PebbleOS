/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file util/bitset.h
//!
//! Helper functions for dealing with a bitsets of various widths.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "system/passert.h"

static inline void bitset8_set(uint8_t* bitset, unsigned int index) {
  bitset[index / 8] |= (1 << (index % 8));
}

static inline void bitset8_clear(uint8_t* bitset, unsigned int index) {
  bitset[index / 8] &= ~(1 << (index % 8));
}

static inline void bitset8_update(uint8_t* bitset, unsigned int index, bool value) {
  if (value) {
    bitset8_set(bitset, index);
  } else {
    bitset8_clear(bitset, index);
  }
}

static inline bool bitset8_get(const uint8_t* bitset, unsigned int index) {
  return bitset[index / 8] & (1 << (index % 8));
}

static inline void bitset16_set(uint16_t* bitset, unsigned int index) {
  bitset[index / 16] |= (1 << (index % 16));
}

static inline void bitset16_clear(uint16_t* bitset, unsigned int index) {
  bitset[index / 16] &= ~(1 << (index % 16));
}

static inline void bitset16_update(uint16_t* bitset, unsigned int index, bool value) {
  if (value) {
    bitset16_set(bitset, index);
  } else {
    bitset16_clear(bitset, index);
  }
}

static inline bool bitset16_get(const uint16_t* bitset, unsigned int index) {
  return bitset[index / 16] & (1 << (index % 16));
}

static inline void bitset32_set(uint32_t* bitset, unsigned int index) {
  bitset[index / 32] |= (1 << (index % 32));
}

static inline void bitset32_clear(uint32_t* bitset, unsigned int index) {
  bitset[index / 32] &= ~(1 << (index % 32));
}

static inline void bitset32_clear_all(uint32_t* bitset, unsigned int width) {
  if (width > 32) {
    // TODO: revisit
    WTF;
  }
  *bitset &= ~((1 << (width + 1)) - 1);
}

static inline void bitset32_update(uint32_t* bitset, unsigned int index, bool value) {
  if (value) {
    bitset32_set(bitset, index);
  } else {
    bitset32_clear(bitset, index);
  }
}

static inline bool bitset32_get(const uint32_t* bitset, unsigned int index) {
  return bitset[index / 32] & (1 << (index % 32));
}

#ifdef __arm__
#define rotl32(x, shift) \
__asm__ volatile ("ror %0,%0,%1" : "+r" (src) : "r" (32 - (shift)) :);
#else
#define rotl32(x, shift) \
uint32_t s = shift % 32; \
{x = ((x << (s)) | x >> (32 - (s)));}
#endif

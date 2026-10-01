/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)&(((type *)0)->member)))

#define PBL_SWAP(a, b)                 \
  do {                                 \
    __typeof__(a) _pbl_swap_tmp = (a); \
    (a) = (b);                         \
    (b) = _pbl_swap_tmp;               \
  } while (0)

//! Packs four characters into a uint32_t, first character in the most significant byte.
#define PBL_FOURCC(a, b, c, d)                                                  \
  ((uint32_t)(((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
              ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d)))

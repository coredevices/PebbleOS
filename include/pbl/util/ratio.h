/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! A ratio32 is a uint32_t holding a real number in [0, 1], scaled by PBL_RATIO32_MAX.
#define PBL_RATIO32_MAX 65535

//! Converts a ratio32 to a percent in the range [0, 100]
static inline uint32_t pbl_ratio32_to_percent(uint32_t ratio) {
  return (ratio * 100) / PBL_RATIO32_MAX;
}

//! Converts a percent in the range [0, 100] to a ratio32
static inline uint32_t pbl_ratio32_from_percent(uint32_t percent) {
  return (percent * PBL_RATIO32_MAX) / 100 + 1;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup util_ratio Ratios
 * @ingroup util
 * @brief Real numbers in [0, 1] stored as scaled integers.
 * @{
 */

/** @brief Scale of a ratio32, a @c uint32_t holding a real number in [0, 1]. */
#define PBL_RATIO32_MAX 65535

/**
 * @brief Convert a ratio32 to a percentage.
 *
 * @param ratio Ratio32, 0 to @ref PBL_RATIO32_MAX.
 * @return Percentage, 0 to 100, truncated.
 */
static inline uint32_t pbl_ratio32_to_percent(uint32_t ratio) {
  return (ratio * 100) / PBL_RATIO32_MAX;
}

/**
 * @brief Convert a percentage to a ratio32.
 *
 * The result is one more than the truncated value, so it converts back to @p percent; 100
 * gives @ref PBL_RATIO32_MAX + 1.
 *
 * @param percent Percentage, 0 to 100.
 * @return Ratio32.
 */
static inline uint32_t pbl_ratio32_from_percent(uint32_t percent) {
  return (percent * PBL_RATIO32_MAX) / 100 + 1;
}

/** @} */

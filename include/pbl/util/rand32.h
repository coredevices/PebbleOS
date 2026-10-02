/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

/**
 * @defgroup util_rand32 Random numbers
 * @ingroup util
 * @brief Random number source used by the library.
 * @{
 */

/**
 * @brief Get a 32-bit random number.
 *
 * The library provides a weak implementation based on rand(); the firmware overrides it.
 *
 * @return Random number.
 */
extern uint32_t pbl_rand32(void);

/** @} */

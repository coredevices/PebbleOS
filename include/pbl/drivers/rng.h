/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup drivers_rng RNG
 * @ingroup drivers
 * @brief Hardware random number generator.
 * @{
 */

/**
 * @brief Generate a 32-bit random number.
 *
 * @param[out] rand_out Random number.
 * @return true on success, false if no hardware RNG is available.
 */
bool rng_rand(uint32_t *rand_out);

/** @} */

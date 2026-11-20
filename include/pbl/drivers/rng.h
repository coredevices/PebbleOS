/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

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
 * @return 0 or a negative errno.
 */
int rng_rand(uint32_t *rand_out);

/** @} */

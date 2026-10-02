/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup util_hash Hashing
 * @ingroup util
 * @brief Non-cryptographic hash.
 * @{
 */

/**
 * @brief Hash bytes with the DJB2 algorithm.
 *
 * @param bytes Data.
 * @param length Length of @p bytes.
 * @return Hash, 5381 for empty data.
 */
uint32_t hash(const uint8_t *bytes, const uint32_t length);

/** @} */

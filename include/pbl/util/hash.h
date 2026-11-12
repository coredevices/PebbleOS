/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
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

/** Initial value for hash_update(). */
#define HASH_INIT 5381U

/**
 * @brief Continue a DJB2 hash with more data.
 *
 * Hashing data in chunks gives the same result as hash() on all of it:
 *
 * @code{.c}
 * uint32_t h = hash_update(HASH_INIT, &hdr, sizeof(hdr));
 * h = hash_update(h, payload, payload_len);
 * @endcode
 *
 * @param h Running hash, HASH_INIT to start.
 * @param data Data.
 * @param len Length of @p data.
 * @return Updated hash.
 */
uint32_t hash_update(uint32_t h, const void *data, size_t len);

/** @} */

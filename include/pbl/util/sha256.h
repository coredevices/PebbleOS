/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_sha256 SHA-256
 * @ingroup util
 * @brief SHA-256 digest, for keying data by content.
 *
 * @code{.c}
 * Sha256 sha;
 * uint8_t digest[SHA256_DIGEST_SIZE];
 *
 * sha256_init(&sha);
 * sha256_update(&sha, title, strlen(title));
 * sha256_final(&sha, digest);
 * @endcode
 * @{
 */

/** @brief Size of a digest, in bytes. */
#define SHA256_DIGEST_SIZE 32

/** @brief Digest state. */
typedef struct {
  /** Hash state. */
  uint32_t state[8];
  /** Number of bytes hashed. */
  uint64_t length;
  /** Pending input block. */
  uint8_t block[64];
} Sha256;

/**
 * @brief Start a digest.
 *
 * @param[out] sha State.
 */
void sha256_init(Sha256 *sha);

/**
 * @brief Hash data.
 *
 * @param sha State.
 * @param data Data.
 * @param length Length of @p data.
 */
void sha256_update(Sha256 *sha, const void *data, size_t length);

/**
 * @brief Finish a digest.
 *
 * @param sha State, which must be initialized again before reuse.
 * @param[out] digest Digest.
 */
void sha256_final(Sha256 *sha, uint8_t digest[SHA256_DIGEST_SIZE]);

/** @} */

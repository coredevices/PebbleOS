/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup crypto Cryptography
 * @ingroup subsys
 * @brief Hashes and ciphers.
 */

/**
 * @defgroup crypto_sha256 SHA-256
 * @ingroup crypto
 * @brief SHA-256 digest (FIPS 180-4).
 *
 * Runs of at least @c CONFIG_CRYPTO_SHA256_HW_MIN_LEN bytes of whole blocks are
 * hashed by a hardware accelerator when a driver provides one (see @ref drivers_crypto), the rest
 * in software.
 *
 * @code{.c}
 * struct pbl_sha256_ctx ctx;
 * uint8_t digest[PBL_SHA256_DIGEST_SIZE];
 *
 * pbl_sha256_init(&ctx);
 * pbl_sha256_update(&ctx, title, strlen(title));
 * pbl_sha256_update(&ctx, artist, strlen(artist));
 * pbl_sha256_final(&ctx, digest);
 * @endcode
 * @{
 */

/** @brief Size of a digest, in bytes. */
#define PBL_SHA256_DIGEST_SIZE 32U

/** @brief Size of a block, in bytes. */
#define PBL_SHA256_BLOCK_SIZE 64U

/** @brief Digest state. */
struct pbl_sha256_ctx {
  /** Chaining value. */
  uint32_t state[8];
  /** Number of bytes hashed. */
  uint64_t count;
  /** Pending partial block. */
  uint8_t buf[PBL_SHA256_BLOCK_SIZE];
};

/**
 * @brief Start a digest.
 *
 * @param[out] ctx Digest state.
 */
void pbl_sha256_init(struct pbl_sha256_ctx *ctx);

/**
 * @brief Hash data.
 *
 * @param ctx Digest state.
 * @param data Data to hash.
 * @param len Length of @p data in bytes.
 */
void pbl_sha256_update(struct pbl_sha256_ctx *ctx, const void *data, size_t len);

/**
 * @brief Finish a digest.
 *
 * The state is cleared, and must be started again before reuse.
 *
 * @param ctx Digest state.
 * @param[out] digest Digest of all data hashed since pbl_sha256_init().
 */
void pbl_sha256_final(struct pbl_sha256_ctx *ctx, uint8_t digest[PBL_SHA256_DIGEST_SIZE]);

/**
 * @brief Digest of a single buffer.
 *
 * @param data Data to hash.
 * @param len Length of @p data in bytes.
 * @param[out] digest Digest.
 */
void pbl_sha256(const void *data, size_t len, uint8_t digest[PBL_SHA256_DIGEST_SIZE]);

/**
 * @brief Compress whole blocks in software.
 *
 * Bypasses any accelerator, to check drivers against.
 *
 * @param[in,out] state Chaining value, H0 to H7.
 * @param data Blocks to hash.
 * @param nblocks Number of blocks in @p data.
 */
void pbl_sha256_blocks_generic(uint32_t state[8], const void *data, size_t nblocks);

/** @} */

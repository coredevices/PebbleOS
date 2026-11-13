/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup drivers_crypto Crypto
 * @ingroup drivers
 * @brief Hardware hash and cipher acceleration used by @ref crypto.
 *
 * SHA-256 is provided by drivers that select @c CRYPTO_SHA256_HW_ACCEL, AES by drivers that
 * select @c CRYPTO_AES_HW_ACCEL.
 *
 * Each function processes a prefix of the blocks and returns how many. A driver processes none
 * when the hardware cannot take the request, before it is ready or for any other reason; the
 * crypto subsystem processes whatever is left in software.
 * @{
 */

/**
 * @brief Accelerated SHA-256 compression.
 *
 * Compresses blocks into a running chaining value, without padding.
 *
 * @param[in,out] state Chaining value, H0 to H7.
 * @param data Blocks to hash.
 * @param nblocks Number of 64-byte blocks in @p data.
 * @return Number of blocks processed; @p state reflects exactly those.
 */
size_t pbl_crypto_hw_sha256_blocks(uint32_t state[8], const void *data, size_t nblocks);

/**
 * @brief Accelerated AES encryption in ECB mode.
 *
 * @param key Key.
 * @param key_len Length of @p key in bytes: 16, 24 or 32.
 * @param[out] out Ciphertext, which may be @p in.
 * @param in Plaintext.
 * @param nblocks Number of 16-byte blocks in @p in.
 * @return Number of blocks processed.
 */
size_t pbl_crypto_hw_aes_ecb_encrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks);

/**
 * @brief Accelerated AES decryption in ECB mode.
 *
 * @param key Key.
 * @param key_len Length of @p key in bytes: 16, 24 or 32.
 * @param[out] out Plaintext, which may be @p in.
 * @param in Ciphertext.
 * @param nblocks Number of 16-byte blocks in @p in.
 * @return Number of blocks processed.
 */
size_t pbl_crypto_hw_aes_ecb_decrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks);

/** @} */

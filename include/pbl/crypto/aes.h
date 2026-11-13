/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup crypto_aes AES
 * @ingroup crypto
 * @brief AES block cipher (FIPS 197), on single blocks or in ECB mode.
 *
 * Runs of at least @c CONFIG_CRYPTO_AES_HW_MIN_LEN bytes are processed by a hardware accelerator
 * when a driver provides one (see @ref drivers_crypto), the rest in software. Other modes of
 * operation are built by the caller.
 *
 * @code{.c}
 * struct pbl_aes_ctx ctx;
 *
 * ret = pbl_aes_expandkey(&ctx, key, PBL_AES_KEYSIZE_128);
 * ret = pbl_aes_ecb_encrypt(&ctx, buf, buf, len);
 * @endcode
 * @{
 */

/** @brief Size of a block, in bytes. */
#define PBL_AES_BLOCK_SIZE 16U

/** @brief Size of an AES-128 key, in bytes. */
#define PBL_AES_KEYSIZE_128 16U
/** @brief Size of an AES-192 key, in bytes. */
#define PBL_AES_KEYSIZE_192 24U
/** @brief Size of an AES-256 key, in bytes. */
#define PBL_AES_KEYSIZE_256 32U

/** @brief Most 32-bit words in a key schedule, for AES-256. */
#define PBL_AES_MAX_KEYLENGTH_U32 60U

/** @brief Expanded key. */
struct pbl_aes_ctx {
  /** Encryption round keys; the first words are the key itself. */
  uint32_t key_enc[PBL_AES_MAX_KEYLENGTH_U32];
  /** Decryption round keys. */
  uint32_t key_dec[PBL_AES_MAX_KEYLENGTH_U32];
  /** Key length in bytes. */
  uint32_t key_length;
};

/**
 * @brief Expand a key.
 *
 * @param[out] ctx Expanded key.
 * @param key Key.
 * @param key_len Length of @p key in bytes: @ref PBL_AES_KEYSIZE_128, @ref PBL_AES_KEYSIZE_192
 *                or @ref PBL_AES_KEYSIZE_256.
 * @retval 0 on success.
 * @retval -EINVAL if @p key_len is not a valid key size.
 */
int pbl_aes_expandkey(struct pbl_aes_ctx *ctx, const uint8_t *key, size_t key_len);

/**
 * @brief Encrypt a block.
 *
 * @param ctx Expanded key.
 * @param[out] out Ciphertext, which may be @p in.
 * @param in Plaintext.
 */
void pbl_aes_encrypt(const struct pbl_aes_ctx *ctx, uint8_t out[PBL_AES_BLOCK_SIZE],
                     const uint8_t in[PBL_AES_BLOCK_SIZE]);

/**
 * @brief Decrypt a block.
 *
 * @param ctx Expanded key.
 * @param[out] out Plaintext, which may be @p in.
 * @param in Ciphertext.
 */
void pbl_aes_decrypt(const struct pbl_aes_ctx *ctx, uint8_t out[PBL_AES_BLOCK_SIZE],
                     const uint8_t in[PBL_AES_BLOCK_SIZE]);

/**
 * @brief Encrypt blocks in ECB mode.
 *
 * @param ctx Expanded key.
 * @param[out] out Ciphertext, which may be @p in.
 * @param in Plaintext.
 * @param len Length of @p in in bytes, a multiple of @ref PBL_AES_BLOCK_SIZE.
 * @retval 0 on success.
 * @retval -EINVAL if @p len is not a multiple of @ref PBL_AES_BLOCK_SIZE.
 */
int pbl_aes_ecb_encrypt(const struct pbl_aes_ctx *ctx, uint8_t *out, const uint8_t *in, size_t len);

/**
 * @brief Decrypt blocks in ECB mode.
 *
 * @param ctx Expanded key.
 * @param[out] out Plaintext, which may be @p in.
 * @param in Ciphertext.
 * @param len Length of @p in in bytes, a multiple of @ref PBL_AES_BLOCK_SIZE.
 * @retval 0 on success.
 * @retval -EINVAL if @p len is not a multiple of @ref PBL_AES_BLOCK_SIZE.
 */
int pbl_aes_ecb_decrypt(const struct pbl_aes_ctx *ctx, uint8_t *out, const uint8_t *in, size_t len);

/** @} */

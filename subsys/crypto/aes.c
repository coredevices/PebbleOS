/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Blocks run on round keys loaded into a temporary mbedTLS context.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "hw.h"

#include <errno.h>
#include <string.h>

#include <pbl/crypto/aes.h>
#include <pbl/drivers/crypto.h>

#include <mbedtls/aes.h>

static_assert(sizeof(((mbedtls_aes_context *)0)->buf) >=
              sizeof(((struct pbl_aes_ctx *)0)->key_enc));

static size_t prv_rounds(size_t key_len) {
  return key_len / 4U + 6U;
}

static size_t prv_schedule_words(size_t key_len) {
  return (prv_rounds(key_len) + 1U) * 4U;
}

static void prv_store(uint32_t *round_keys, const mbedtls_aes_context *mbed, size_t key_len) {
  memcpy(round_keys, mbed->buf + mbed->rk_offset, prv_schedule_words(key_len) * sizeof(uint32_t));
}

int pbl_aes_expandkey(struct pbl_aes_ctx *ctx, const uint8_t *key, size_t key_len) {
  mbedtls_aes_context mbed;
  int ret = 0;

  if ((key_len != PBL_AES_KEYSIZE_128) && (key_len != PBL_AES_KEYSIZE_192) &&
      (key_len != PBL_AES_KEYSIZE_256)) {
    return -EINVAL;
  }

  mbedtls_aes_init(&mbed);
  if (mbedtls_aes_setkey_enc(&mbed, key, key_len * 8U) != 0) {
    ret = -EINVAL;
    goto out;
  }
  prv_store(ctx->key_enc, &mbed, key_len);

  if (mbedtls_aes_setkey_dec(&mbed, key, key_len * 8U) != 0) {
    ret = -EINVAL;
    goto out;
  }
  prv_store(ctx->key_dec, &mbed, key_len);

  ctx->key_length = key_len;

out:
  mbedtls_aes_free(&mbed);
  return ret;
}

static void prv_ecb_sw(const struct pbl_aes_ctx *ctx, bool encrypt, uint8_t *out, const uint8_t *in,
                       size_t nblocks) {
  mbedtls_aes_context mbed;

  // Only the rounds and round keys are read: no init or zeroing, which would cost more than the
  // block itself.
  mbed.nr = (int)prv_rounds(ctx->key_length);
  mbed.rk_offset = 0;
  memcpy(mbed.buf, encrypt ? ctx->key_enc : ctx->key_dec,
         prv_schedule_words(ctx->key_length) * sizeof(uint32_t));

  for (; nblocks > 0U; nblocks--) {
    if (encrypt) {
      (void)mbedtls_internal_aes_encrypt(&mbed, in, out);
    } else {
      (void)mbedtls_internal_aes_decrypt(&mbed, in, out);
    }
    in += PBL_AES_BLOCK_SIZE;
    out += PBL_AES_BLOCK_SIZE;
  }
}
#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
static bool prv_hw_usable(void) {
#if defined(CONFIG_CRYPTO_SELFTEST)
  return pbl_crypto__hw_usable(PBL_CRYPTO_ALG_AES);
#else
  return true;
#endif
}
#endif

static int prv_ecb(const struct pbl_aes_ctx *ctx, bool encrypt, uint8_t *out, const uint8_t *in,
                   size_t len) {
  size_t nblocks = len / PBL_AES_BLOCK_SIZE;

  if ((len % PBL_AES_BLOCK_SIZE) != 0U) {
    return -EINVAL;
  }

#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
  if ((len >= CONFIG_CRYPTO_AES_HW_MIN_LEN) && prv_hw_usable()) {
    // The first words of the encryption schedule are the key.
    const uint8_t *key = (const uint8_t *)ctx->key_enc;
    const size_t done = encrypt
                            ? pbl_crypto_hw_aes_ecb_encrypt(key, ctx->key_length, out, in, nblocks)
                            : pbl_crypto_hw_aes_ecb_decrypt(key, ctx->key_length, out, in, nblocks);

    out += done * PBL_AES_BLOCK_SIZE;
    in += done * PBL_AES_BLOCK_SIZE;
    nblocks -= done;
  }
#endif

  if (nblocks > 0U) {
    prv_ecb_sw(ctx, encrypt, out, in, nblocks);
  }

  return 0;
}

void pbl_aes_encrypt(const struct pbl_aes_ctx *ctx, uint8_t out[PBL_AES_BLOCK_SIZE],
                     const uint8_t in[PBL_AES_BLOCK_SIZE]) {
  (void)prv_ecb(ctx, true, out, in, PBL_AES_BLOCK_SIZE);
}

void pbl_aes_decrypt(const struct pbl_aes_ctx *ctx, uint8_t out[PBL_AES_BLOCK_SIZE],
                     const uint8_t in[PBL_AES_BLOCK_SIZE]) {
  (void)prv_ecb(ctx, false, out, in, PBL_AES_BLOCK_SIZE);
}

int pbl_aes_ecb_encrypt(const struct pbl_aes_ctx *ctx, uint8_t *out, const uint8_t *in,
                        size_t len) {
  return prv_ecb(ctx, true, out, in, len);
}

int pbl_aes_ecb_decrypt(const struct pbl_aes_ctx *ctx, uint8_t *out, const uint8_t *in,
                        size_t len) {
  return prv_ecb(ctx, false, out, in, len);
}

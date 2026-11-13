/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The software compression runs on a chaining value loaded into a temporary mbedTLS context.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "hw.h"

#include <string.h>

#include <pbl/crypto/sha256.h>
#include <pbl/drivers/crypto.h>

#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>

void pbl_sha256_blocks_generic(uint32_t state[8], const void *data, size_t nblocks) {
  const uint8_t *bytes = data;
  mbedtls_sha256_context mbed;

  mbedtls_sha256_init(&mbed);
  memcpy(mbed.state, state, sizeof(mbed.state));
  for (; nblocks > 0U; nblocks--) {
    (void)mbedtls_internal_sha256_process(&mbed, bytes);
    bytes += PBL_SHA256_BLOCK_SIZE;
  }
  memcpy(state, mbed.state, sizeof(mbed.state));
  mbedtls_sha256_free(&mbed);
}

#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
static bool prv_hw_usable(void) {
#if defined(CONFIG_CRYPTO_SELFTEST)
  return pbl_crypto__hw_usable(PBL_CRYPTO_ALG_SHA256);
#else
  return true;
#endif
}
#endif

static void prv_blocks(uint32_t state[8], const uint8_t *data, size_t nblocks) {
#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
  if ((nblocks * PBL_SHA256_BLOCK_SIZE >= CONFIG_CRYPTO_SHA256_HW_MIN_LEN) && prv_hw_usable()) {
    const size_t done = pbl_crypto_hw_sha256_blocks(state, data, nblocks);
    data += done * PBL_SHA256_BLOCK_SIZE;
    nblocks -= done;
  }
#endif
  if (nblocks > 0U) {
    pbl_sha256_blocks_generic(state, data, nblocks);
  }
}

void pbl_sha256_init(struct pbl_sha256_ctx *ctx) {
  static const uint32_t h0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };

  memcpy(ctx->state, h0, sizeof(h0));
  ctx->count = 0;
}

void pbl_sha256_update(struct pbl_sha256_ctx *ctx, const void *data, size_t len) {
  const uint8_t *bytes = data;
  size_t used = ctx->count % PBL_SHA256_BLOCK_SIZE;

  if (len == 0U) {
    return;
  }

  ctx->count += len;

  if (used > 0U) {
    const size_t n = (len < PBL_SHA256_BLOCK_SIZE - used) ? len : PBL_SHA256_BLOCK_SIZE - used;

    memcpy(&ctx->buf[used], bytes, n);
    bytes += n;
    len -= n;
    used += n;
    if (used < PBL_SHA256_BLOCK_SIZE) {
      return;
    }
    prv_blocks(ctx->state, ctx->buf, 1);
  }

  const size_t nblocks = len / PBL_SHA256_BLOCK_SIZE;
  prv_blocks(ctx->state, bytes, nblocks);
  bytes += nblocks * PBL_SHA256_BLOCK_SIZE;
  len -= nblocks * PBL_SHA256_BLOCK_SIZE;

  memcpy(ctx->buf, bytes, len);
}

void pbl_sha256_final(struct pbl_sha256_ctx *ctx, uint8_t digest[PBL_SHA256_DIGEST_SIZE]) {
  const uint64_t bits = ctx->count * 8U;
  size_t used = ctx->count % PBL_SHA256_BLOCK_SIZE;

  ctx->buf[used++] = 0x80;
  if (used > PBL_SHA256_BLOCK_SIZE - 8U) {
    memset(&ctx->buf[used], 0, PBL_SHA256_BLOCK_SIZE - used);
    prv_blocks(ctx->state, ctx->buf, 1);
    used = 0;
  }
  memset(&ctx->buf[used], 0, PBL_SHA256_BLOCK_SIZE - 8U - used);
  for (unsigned int i = 0; i < 8U; i++) {
    ctx->buf[PBL_SHA256_BLOCK_SIZE - 8U + i] = (uint8_t)(bits >> (56U - 8U * i));
  }
  prv_blocks(ctx->state, ctx->buf, 1);

  for (unsigned int i = 0; i < 8U; i++) {
    digest[4U * i] = (uint8_t)(ctx->state[i] >> 24);
    digest[4U * i + 1U] = (uint8_t)(ctx->state[i] >> 16);
    digest[4U * i + 2U] = (uint8_t)(ctx->state[i] >> 8);
    digest[4U * i + 3U] = (uint8_t)ctx->state[i];
  }

  mbedtls_platform_zeroize(ctx, sizeof(*ctx));
}

void pbl_sha256(const void *data, size_t len, uint8_t digest[PBL_SHA256_DIGEST_SIZE]) {
  struct pbl_sha256_ctx ctx;

  pbl_sha256_init(&ctx);
  pbl_sha256_update(&ctx, data, len);
  pbl_sha256_final(&ctx, digest);
}

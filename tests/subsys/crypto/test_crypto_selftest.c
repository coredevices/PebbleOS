/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/crypto/aes.h>
#include <pbl/crypto/selftest.h>
#include <pbl/crypto/sha256.h>
#include <pbl/drivers/crypto.h>

#include <clar.h>

static uint8_t s_data[256];
static unsigned int s_sha256_calls;
static unsigned int s_aes_calls;

// Single blocks stay below the threshold, so the subsystem computes them in software.
static size_t prv_aes(bool encrypt, const uint8_t *key, size_t key_len, uint8_t *out,
                      const uint8_t *in, size_t nblocks) {
  struct pbl_aes_ctx ctx;

  s_aes_calls++;
#if defined(FAKE_HW_absent)
  return 0;
#endif
  cl_assert_equal_i(pbl_aes_expandkey(&ctx, key, key_len), 0);
  for (size_t i = 0; i < nblocks; i++) {
    if (encrypt) {
      pbl_aes_encrypt(&ctx, &out[16U * i], &in[16U * i]);
    } else {
      pbl_aes_decrypt(&ctx, &out[16U * i], &in[16U * i]);
    }
  }
  return nblocks;
}

size_t pbl_crypto_hw_sha256_blocks(uint32_t state[8], const void *data, size_t nblocks) {
  s_sha256_calls++;
#if defined(FAKE_HW_absent)
  return 0;
#endif
  pbl_sha256_blocks_generic(state, data, nblocks);
#if defined(FAKE_HW_wrong)
  state[0] ^= 1U;
#endif
  return nblocks;
}

size_t pbl_crypto_hw_aes_ecb_encrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_aes(true, key, key_len, out, in, nblocks);
}

size_t pbl_crypto_hw_aes_ecb_decrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_aes(false, key, key_len, out, in, nblocks);
}

static void prv_ref_sha256(const uint8_t *data, size_t len, uint8_t digest[32]) {
  uint32_t h[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  uint8_t tail[128] = {0};
  const size_t full = len / 64U;
  const size_t rem = len - full * 64U;
  const size_t tail_len = (rem < 56U) ? 64U : 128U;

  pbl_sha256_blocks_generic(h, data, full);
  memcpy(tail, &data[full * 64U], rem);
  tail[rem] = 0x80;
  for (unsigned int i = 0; i < 8U; i++) {
    tail[tail_len - 1U - i] = (uint8_t)(((uint64_t)len * 8U) >> (8U * i));
  }
  pbl_sha256_blocks_generic(h, tail, tail_len / 64U);
  for (unsigned int i = 0; i < 8U; i++) {
    digest[4U * i] = (uint8_t)(h[i] >> 24);
    digest[4U * i + 1U] = (uint8_t)(h[i] >> 16);
    digest[4U * i + 2U] = (uint8_t)(h[i] >> 8);
    digest[4U * i + 3U] = (uint8_t)h[i];
  }
}

void test_crypto_selftest__initialize(void) {
  for (size_t i = 0; i < sizeof(s_data); i++) {
    s_data[i] = (uint8_t)(i * 7U + 3U);
  }
}

// One test per binary: the self-test result lasts for the life of the program.
void test_crypto_selftest__fake_accelerator(void) {
  uint8_t expected[PBL_SHA256_DIGEST_SIZE];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];
  struct pbl_aes_ctx ctx;
  uint8_t ecb[64];
  uint8_t ref[64];

  // First use runs the self-test; results always match software.
  prv_ref_sha256(s_data, sizeof(s_data), expected);
  pbl_sha256(s_data, sizeof(s_data), digest);
  cl_assert_equal_m(digest, expected, sizeof(digest));

  cl_assert_equal_i(pbl_aes_expandkey(&ctx, s_data, PBL_AES_KEYSIZE_128), 0);
  for (size_t i = 0; i < sizeof(ref); i += PBL_AES_BLOCK_SIZE) {
    pbl_aes_encrypt(&ctx, &ref[i], &s_data[i]);
  }
  cl_assert_equal_i(s_aes_calls, 0);
  cl_assert_equal_i(pbl_aes_ecb_encrypt(&ctx, ecb, s_data, sizeof(ecb)), 0);
  cl_assert_equal_m(ecb, ref, sizeof(ecb));

#if defined(FAKE_HW_ok)
  // Three self-test requests, then the data
  cl_assert(s_sha256_calls > 3);
  // Six self-test requests, then the data
  cl_assert_equal_i(s_aes_calls, 7);
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_SHA256), 0);
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_AES), 0);
#elif defined(FAKE_HW_wrong)
  // The first self-test request fails, and SHA-256 stays in software
  cl_assert_equal_i(s_sha256_calls, 1);
  cl_assert_equal_i(s_aes_calls, 7);
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_SHA256), -EIO);
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_AES), 0);
  s_sha256_calls = 0;
  pbl_sha256(s_data, sizeof(s_data), digest);
  cl_assert_equal_m(digest, expected, sizeof(digest));
  cl_assert_equal_i(s_sha256_calls, 0);
#else
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_SHA256), -ENODEV);
  cl_assert_equal_i(pbl_crypto_selftest(PBL_CRYPTO_ALG_AES), -ENODEV);
#endif
}

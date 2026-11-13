/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/crypto/aes.h>
#include <pbl/drivers/crypto.h>

#include <clar.h>
static uint8_t s_data[1024];
static bool s_hw_available;
static unsigned int s_hw_calls;
static size_t s_hw_blocks;

// Single blocks stay below the threshold, so the subsystem computes them in software.
static size_t prv_hw(bool encrypt, const uint8_t *key, size_t key_len, uint8_t *out,
                     const uint8_t *in, size_t nblocks) {
  struct pbl_aes_ctx ctx;

  s_hw_calls++;
  if (!s_hw_available) {
    return 0;
  }

  const size_t done = (nblocks + 1U) / 2U;
  cl_assert_equal_i(pbl_aes_expandkey(&ctx, key, key_len), 0);
  for (size_t i = 0; i < done; i++) {
    if (encrypt) {
      pbl_aes_encrypt(&ctx, &out[16U * i], &in[16U * i]);
    } else {
      pbl_aes_decrypt(&ctx, &out[16U * i], &in[16U * i]);
    }
  }
  s_hw_blocks += done;

  return done;
}

size_t pbl_crypto_hw_aes_ecb_encrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_hw(true, key, key_len, out, in, nblocks);
}

size_t pbl_crypto_hw_aes_ecb_decrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_hw(false, key, key_len, out, in, nblocks);
}

static void prv_roundtrip(size_t len) {
  struct pbl_aes_ctx ctx;
  static uint8_t expected[sizeof(s_data)];
  static uint8_t buf[sizeof(s_data)];

  for (size_t key_len = 16; key_len <= 32; key_len += 8) {
    cl_assert_equal_i(pbl_aes_expandkey(&ctx, &s_data[100], key_len), 0);
    for (size_t i = 0; i < len; i += PBL_AES_BLOCK_SIZE) {
      pbl_aes_encrypt(&ctx, &expected[i], &s_data[i]);
    }

    cl_assert_equal_i(pbl_aes_ecb_encrypt(&ctx, buf, s_data, len), 0);
    cl_assert_equal_m(buf, expected, len);
    cl_assert_equal_i(pbl_aes_ecb_decrypt(&ctx, buf, buf, len), 0);
    cl_assert_equal_m(buf, s_data, len);
  }
}

void test_aes_hw__initialize(void) {
  uint32_t x = 0xcafef00d;
  for (size_t i = 0; i < sizeof(s_data); i++) {
    x = x * 1103515245U + 12345U;
    s_data[i] = (uint8_t)(x >> 16);
  }
  s_hw_available = true;
  s_hw_calls = 0;
  s_hw_blocks = 0;
}

void test_aes_hw__partial_offload(void) {
  prv_roundtrip(sizeof(s_data));
  cl_assert_equal_i(s_hw_calls, 6);
  cl_assert(s_hw_blocks > 0);
}

void test_aes_hw__short_runs_stay_in_software(void) {
  struct pbl_aes_ctx ctx;
  uint8_t block[PBL_AES_BLOCK_SIZE];

  prv_roundtrip(16);
  cl_assert_equal_i(pbl_aes_expandkey(&ctx, s_data, 16), 0);
  pbl_aes_encrypt(&ctx, block, s_data);
  pbl_aes_decrypt(&ctx, block, block);
  cl_assert_equal_m(block, s_data, sizeof(block));
  cl_assert_equal_i(s_hw_calls, 0);
}

void test_aes_hw__unavailable_falls_back(void) {
  s_hw_available = false;
  prv_roundtrip(sizeof(s_data));
  cl_assert_equal_i(s_hw_calls, 6);
  cl_assert_equal_i(s_hw_blocks, 0);
}

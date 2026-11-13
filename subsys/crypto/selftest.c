/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hw.h"

#include <errno.h>
#include <string.h>

#include <pbl/crypto/aes.h>
#include <pbl/crypto/selftest.h>
#include <pbl/crypto/sha256.h>
#include <pbl/drivers/crypto.h>
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(crypto, CONFIG_CRYPTO_LOG_LEVEL);

enum state {
  STATE_UNTESTED,
  STATE_OK,
  STATE_FAILED,
};

static volatile enum state s_state[2];

#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
static int prv_check_sha256(void) {
  // FIPS 180-4: "abc", and the 448-bit message in one request and resumed across two
  static const uint32_t h0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  static const uint32_t abc[8] = {
    0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad,
  };
  static const uint32_t two[8] = {
    0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039, 0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1,
  };
  static const char msg[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  uint8_t blocks[2 * PBL_SHA256_BLOCK_SIZE] = {0};
  uint32_t whole[8];
  uint32_t resumed[8];

  memcpy(blocks, "abc", 3);
  blocks[3] = 0x80;
  blocks[PBL_SHA256_BLOCK_SIZE - 1U] = 3U * 8U;
  memcpy(whole, h0, sizeof(whole));
  if (pbl_crypto_hw_sha256_blocks(whole, blocks, 1) != 1) {
    return -ENODEV;
  }
  if (memcmp(whole, abc, sizeof(abc)) != 0) {
    return -EIO;
  }

  memset(blocks, 0, sizeof(blocks));
  memcpy(blocks, msg, sizeof(msg) - 1U);
  blocks[sizeof(msg) - 1U] = 0x80;
  blocks[sizeof(blocks) - 2U] = (uint8_t)(((sizeof(msg) - 1U) * 8U) >> 8);
  blocks[sizeof(blocks) - 1U] = (uint8_t)((sizeof(msg) - 1U) * 8U);

  memcpy(whole, h0, sizeof(whole));
  memcpy(resumed, h0, sizeof(resumed));
  if ((pbl_crypto_hw_sha256_blocks(whole, blocks, 2) != 2) ||
      (pbl_crypto_hw_sha256_blocks(resumed, blocks, 1) != 1) ||
      (pbl_crypto_hw_sha256_blocks(resumed, &blocks[PBL_SHA256_BLOCK_SIZE], 1) != 1)) {
    return -ENODEV;
  }
  if ((memcmp(whole, two, sizeof(two)) != 0) || (memcmp(resumed, two, sizeof(two)) != 0)) {
    return -EIO;
  }

  return 0;
}
#endif

#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
static int prv_check_aes(void) {
  // FIPS 197 Appendix C
  static const uint8_t key[PBL_AES_KEYSIZE_256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  };
  static const uint8_t plain[PBL_AES_BLOCK_SIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
  };
  static const uint8_t cipher[3][PBL_AES_BLOCK_SIZE] = {
    {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5,
     0x5a},
    {0xdd, 0xa9, 0x7c, 0xa4, 0x86, 0x4c, 0xdf, 0xe0, 0x6e, 0xaf, 0x70, 0xa0, 0xec, 0x0d, 0x71,
     0x91},
    {0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf, 0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60,
     0x89},
  };
  uint8_t block[PBL_AES_BLOCK_SIZE];

  for (size_t i = 0; i < 3U; i++) {
    const size_t key_len = PBL_AES_KEYSIZE_128 + 8U * i;

    if (pbl_crypto_hw_aes_ecb_encrypt(key, key_len, block, plain, 1) != 1) {
      return -ENODEV;
    }
    if (memcmp(block, cipher[i], sizeof(block)) != 0) {
      return -EIO;
    }
    if (pbl_crypto_hw_aes_ecb_decrypt(key, key_len, block, cipher[i], 1) != 1) {
      return -ENODEV;
    }
    if (memcmp(block, plain, sizeof(block)) != 0) {
      return -EIO;
    }
  }

  return 0;
}
#endif

int pbl_crypto_selftest(enum pbl_crypto_alg alg) {
  int ret = -ENODEV;

  switch (alg) {
#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
    case PBL_CRYPTO_ALG_SHA256:
      ret = prv_check_sha256();
      break;
#endif
#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
    case PBL_CRYPTO_ALG_AES:
      ret = prv_check_aes();
      break;
#endif
    default:
      return -ENODEV;
  }

  if (ret == -EIO) {
    PBL_LOG_ERR("Accelerator self-test failed (algorithm %d), using software", alg);
    s_state[alg] = STATE_FAILED;
  } else if (ret == 0) {
    s_state[alg] = STATE_OK;
  }

  return ret;
}

bool pbl_crypto__hw_usable(enum pbl_crypto_alg alg) {
  if (s_state[alg] == STATE_UNTESTED) {
    (void)pbl_crypto_selftest(alg);
  }

  return s_state[alg] != STATE_FAILED;
}

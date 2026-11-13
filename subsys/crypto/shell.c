/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/crypto/aes.h>
#if defined(CONFIG_CRYPTO_SELFTEST)
#include <pbl/crypto/selftest.h>
#endif
#include <pbl/crypto/sha256.h>
#include <pbl/drivers/crypto.h>
#include <pbl/kernel/time.h>
#include <pbl/shell/shell.h>
#include <pbl/util/size.h>

#define CRYPTO_BUF_LEN    4096UL
#define CRYPTO_BENCH_RUNS 20U

static uint8_t s_buf[CRYPTO_BUF_LEN + 1U];

static void prv_fill(uint32_t x) {
  for (size_t i = 0; i < sizeof(s_buf); i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_buf[i] = (uint8_t)x;
  }
}

static int prv_parse_len(const struct pbl_shell *sh, size_t argc, char **argv, size_t *len) {
  unsigned long val = CRYPTO_BUF_LEN;

  if (argc > 1 &&
      (pbl_shell_strtoul(argv[1], &val) != 0 || val < PBL_AES_BLOCK_SIZE || val > CRYPTO_BUF_LEN)) {
    pbl_shell_error(sh, "invalid length '%s' (%u-%lu)", argv[1], PBL_AES_BLOCK_SIZE,
                    CRYPTO_BUF_LEN);
    return -EINVAL;
  }

  *len = val;

  return 0;
}

#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
static uint8_t s_out[2][CRYPTO_BUF_LEN];

// Single blocks stay below the threshold, so the subsystem computes them in software.
static_assert(CONFIG_CRYPTO_AES_HW_MIN_LEN > PBL_AES_BLOCK_SIZE);

static void prv_aes_sw(const struct pbl_aes_ctx *ctx, bool encrypt, uint8_t *out, const uint8_t *in,
                       size_t nblocks) {
  for (size_t i = 0; i < nblocks; i++) {
    if (encrypt) {
      pbl_aes_encrypt(ctx, &out[i * PBL_AES_BLOCK_SIZE], &in[i * PBL_AES_BLOCK_SIZE]);
    } else {
      pbl_aes_decrypt(ctx, &out[i * PBL_AES_BLOCK_SIZE], &in[i * PBL_AES_BLOCK_SIZE]);
    }
  }
}

static size_t prv_aes_hw(const struct pbl_aes_ctx *ctx, bool encrypt, uint8_t *out,
                         const uint8_t *in, size_t nblocks) {
  const uint8_t *key = (const uint8_t *)ctx->key_enc;

  if (encrypt) {
    return pbl_crypto_hw_aes_ecb_encrypt(key, ctx->key_length, out, in, nblocks);
  }
  return pbl_crypto_hw_aes_ecb_decrypt(key, ctx->key_length, out, in, nblocks);
}

static int prv_check_aes(const struct pbl_shell *sh, const char *name, const uint8_t *data,
                         size_t nblocks) {
  struct pbl_aes_ctx ctx;
  unsigned int bad = 0;

  for (size_t key_len = PBL_AES_KEYSIZE_128; key_len <= PBL_AES_KEYSIZE_256; key_len += 8U) {
    if (pbl_aes_expandkey(&ctx, &s_buf[CRYPTO_BUF_LEN - key_len], key_len) != 0) {
      return -EINVAL;
    }
    for (int encrypt = 0; encrypt <= 1; encrypt++) {
      prv_aes_sw(&ctx, encrypt, s_out[0], data, nblocks);
      if ((prv_aes_hw(&ctx, encrypt, s_out[1], data, nblocks) != nblocks) ||
          (memcmp(s_out[0], s_out[1], nblocks * PBL_AES_BLOCK_SIZE) != 0)) {
        bad++;
      }
    }
  }

  pbl_shell_print(sh, "aes %s: %zu blocks, 3 key sizes, both ways %s", name, nblocks,
                  (bad == 0U) ? "ok" : "MISMATCH");

  return (bad == 0U) ? 0 : -EIO;
}

static void prv_bench_aes(const struct pbl_shell *sh, size_t len) {
  struct pbl_aes_ctx ctx;
  uint32_t start;
  uint32_t hw;
  uint32_t sw;

  (void)pbl_aes_expandkey(&ctx, s_buf, PBL_AES_KEYSIZE_128);
  for (int encrypt = 0; encrypt <= 1; encrypt++) {
    const size_t nblocks = len / PBL_AES_BLOCK_SIZE;

    start = pbl_cycle_get_32();
    for (unsigned int i = 0U; i < CRYPTO_BENCH_RUNS; i++) {
      (void)prv_aes_hw(&ctx, encrypt, s_out[0], s_buf, nblocks);
    }
    hw = (pbl_cycle_get_32() - start) / CRYPTO_BENCH_RUNS;

    start = pbl_cycle_get_32();
    for (unsigned int i = 0U; i < CRYPTO_BENCH_RUNS; i++) {
      prv_aes_sw(&ctx, encrypt, s_out[0], s_buf, nblocks);
    }
    sw = (pbl_cycle_get_32() - start) / CRYPTO_BENCH_RUNS;

    pbl_shell_print(sh, "aes-128 %s %zu: hw %" PRIu32 " sw %" PRIu32 " cycles",
                    encrypt ? "enc" : "dec", len, hw, sw);
  }
}
#endif

#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
static const uint32_t s_h0[8] = {
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static int prv_check_sha256(const struct pbl_shell *sh, const char *name, const uint8_t *data,
                            size_t nblocks) {
  uint32_t hw[8];
  uint32_t sw[8];

  memcpy(hw, s_h0, sizeof(hw));
  memcpy(sw, s_h0, sizeof(sw));
  const size_t done = pbl_crypto_hw_sha256_blocks(hw, data, nblocks);
  pbl_sha256_blocks_generic(sw, data, nblocks);

  const bool ok = (done == nblocks) && (memcmp(hw, sw, sizeof(hw)) == 0);
  pbl_shell_print(sh, "sha256 %s: %zu/%zu blocks %s", name, done, nblocks, ok ? "ok" : "MISMATCH");

  return ok ? 0 : -EIO;
}

static void prv_bench_sha256(const struct pbl_shell *sh, size_t len) {
  const size_t nblocks = len / PBL_SHA256_BLOCK_SIZE;
  uint32_t state[8];
  uint32_t start;
  uint32_t hw;
  uint32_t sw;

  memcpy(state, s_h0, sizeof(state));

  start = pbl_cycle_get_32();
  for (unsigned int i = 0U; i < CRYPTO_BENCH_RUNS; i++) {
    (void)pbl_crypto_hw_sha256_blocks(state, s_buf, nblocks);
  }
  hw = (pbl_cycle_get_32() - start) / CRYPTO_BENCH_RUNS;

  start = pbl_cycle_get_32();
  for (unsigned int i = 0U; i < CRYPTO_BENCH_RUNS; i++) {
    pbl_sha256_blocks_generic(state, s_buf, nblocks);
  }
  sw = (pbl_cycle_get_32() - start) / CRYPTO_BENCH_RUNS;

  pbl_shell_print(sh, "sha256 %zu: hw %" PRIu32 " sw %" PRIu32 " cycles", len, hw, sw);
}
#endif

static int prv_cmd_check(const struct pbl_shell *sh, size_t argc, char **argv) {
  size_t len;
  int ret;

  ret = prv_parse_len(sh, argc, argv, &len);
  if (ret != 0) {
    return ret;
  }

  prv_fill((uint32_t)pbl_uptime_ticks() | 1U);
  ret = 0;

#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
  if (len >= PBL_SHA256_BLOCK_SIZE) {
    ret = prv_check_sha256(sh, "aligned", s_buf, len / PBL_SHA256_BLOCK_SIZE);
    if (ret != 0) {
      return ret;
    }

    ret = prv_check_sha256(sh, "unaligned", &s_buf[1], len / PBL_SHA256_BLOCK_SIZE);
    if (ret != 0) {
      return ret;
    }
  }
#endif

#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
  ret = prv_check_aes(sh, "aligned", s_buf, len / PBL_AES_BLOCK_SIZE);
  if (ret != 0) {
    return ret;
  }

  ret = prv_check_aes(sh, "unaligned", &s_buf[1], len / PBL_AES_BLOCK_SIZE);
#endif

  return ret;
}

static void prv_bench(const struct pbl_shell *sh, size_t len) {
#if defined(CONFIG_CRYPTO_SHA256_HW_ACCEL)
  if (len >= PBL_SHA256_BLOCK_SIZE) {
    prv_bench_sha256(sh, len);
  }
#endif
#if defined(CONFIG_CRYPTO_AES_HW_ACCEL)
  prv_bench_aes(sh, len);
#endif
}

static int prv_cmd_bench(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const size_t lens[] = {16, 32, 64, 128, 256, 1024, 4096};
  size_t len;
  int ret;

  prv_fill(0x2545f491U);

  if (argc > 1) {
    ret = prv_parse_len(sh, argc, argv, &len);
    if (ret != 0) {
      return ret;
    }
    prv_bench(sh, len);
    return 0;
  }

  for (size_t i = 0; i < ARRAY_LENGTH(lens); i++) {
    prv_bench(sh, lens[i]);
  }

  return 0;
}

#if defined(CONFIG_CRYPTO_SELFTEST)
static int prv_cmd_selftest(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const struct {
    enum pbl_crypto_alg alg;
    const char *name;
  } algs[] = {
    {PBL_CRYPTO_ALG_SHA256, "sha256"},
    {PBL_CRYPTO_ALG_AES, "aes"},
  };
  int ret = 0;

  for (size_t i = 0; i < ARRAY_LENGTH(algs); i++) {
    const int err = pbl_crypto_selftest(algs[i].alg);

    pbl_shell_print(sh, "%s: %s", algs[i].name,
                    (err == 0) ? "ok" : ((err == -ENODEV) ? "no accelerator" : "FAILED"));
    if ((err != 0) && (ret == 0)) {
      ret = err;
    }
  }

  return ret;
}
#endif

static const struct pbl_shell_cmd sub_crypto[] = {
#if defined(CONFIG_CRYPTO_SELFTEST)
  PBL_SHELL_CMD(selftest, nullptr, "Run the accelerator known-answer tests", prv_cmd_selftest),
#endif
  PBL_SHELL_CMD_ARG(check, nullptr, "Compare the accelerator with software [len]", prv_cmd_check, 1,
                    1),
  PBL_SHELL_CMD_ARG(bench, nullptr, "Time the accelerator and software [len]", prv_cmd_bench, 1, 1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(crypto, sub_crypto, "Crypto subsystem", nullptr);

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/crypto/sha256.h>
#include <pbl/drivers/crypto.h>

#include <clar.h>

static uint8_t s_data[1031];
static bool s_hw_available;
static unsigned int s_hw_calls;
static size_t s_hw_blocks;

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32U - (n))))

static void prv_ref_block(uint32_t h[8], const uint8_t *p) {
  static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  };
  uint32_t w[64];
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];

  for (int t = 0; t < 16; t++) {
    w[t] = ((uint32_t)p[4 * t] << 24) | ((uint32_t)p[4 * t + 1] << 16) |
           ((uint32_t)p[4 * t + 2] << 8) | p[4 * t + 3];
  }
  for (int t = 16; t < 64; t++) {
    w[t] = (ROTR(w[t - 2], 17) ^ ROTR(w[t - 2], 19) ^ (w[t - 2] >> 10)) + w[t - 7] +
           (ROTR(w[t - 15], 7) ^ ROTR(w[t - 15], 18) ^ (w[t - 15] >> 3)) + w[t - 16];
  }
  for (int t = 0; t < 64; t++) {
    uint32_t t1 =
        hh + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + k[t] + w[t];
    uint32_t t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
  h[5] += f;
  h[6] += g;
  h[7] += hh;
}

size_t pbl_crypto_hw_sha256_blocks(uint32_t state[8], const void *data, size_t nblocks) {
  s_hw_calls++;
  if (!s_hw_available) {
    return 0;
  }
  const size_t done = (nblocks + 1) / 2;
  for (size_t i = 0; i < done; i++) {
    prv_ref_block(state, (const uint8_t *)data + i * PBL_SHA256_BLOCK_SIZE);
  }
  s_hw_blocks += done;
  return done;
}

static void prv_ref_sha256(const uint8_t *data, size_t len, uint8_t digest[32]) {
  uint32_t h[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  uint8_t tail[128] = {0};
  const size_t full = len / 64;

  for (size_t i = 0; i < full; i++) {
    prv_ref_block(h, &data[i * 64]);
  }
  const size_t rem = len - full * 64;
  memcpy(tail, &data[full * 64], rem);
  tail[rem] = 0x80;
  const size_t tail_len = (rem < 56) ? 64 : 128;
  const uint64_t bits = (uint64_t)len * 8U;
  for (int i = 0; i < 8; i++) {
    tail[tail_len - 1 - i] = (uint8_t)(bits >> (8 * i));
  }
  for (size_t i = 0; i < tail_len; i += 64) {
    prv_ref_block(h, &tail[i]);
  }
  for (int i = 0; i < 8; i++) {
    digest[4 * i] = (uint8_t)(h[i] >> 24);
    digest[4 * i + 1] = (uint8_t)(h[i] >> 16);
    digest[4 * i + 2] = (uint8_t)(h[i] >> 8);
    digest[4 * i + 3] = (uint8_t)h[i];
  }
}

void test_sha256_hw__initialize(void) {
  uint32_t x = 0xcafef00d;
  for (size_t i = 0; i < sizeof(s_data); i++) {
    x = x * 1103515245U + 12345U;
    s_data[i] = (uint8_t)(x >> 16);
  }
  s_hw_available = true;
  s_hw_calls = 0;
  s_hw_blocks = 0;
}

void test_sha256_hw__sha256_partial_offload(void) {
  uint8_t expected[PBL_SHA256_DIGEST_SIZE];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];

  prv_ref_sha256(s_data, sizeof(s_data), expected);
  for (size_t split = 0; split <= 130; split++) {
    struct pbl_sha256_ctx ctx;

    pbl_sha256_init(&ctx);
    pbl_sha256_update(&ctx, s_data, split);
    pbl_sha256_update(&ctx, &s_data[split], sizeof(s_data) - split);
    pbl_sha256_final(&ctx, digest);
    cl_assert_equal_m(digest, expected, sizeof(expected));
  }
  cl_assert(s_hw_blocks > 0);
}

void test_sha256_hw__sha256_unavailable_falls_back(void) {
  uint8_t expected[PBL_SHA256_DIGEST_SIZE];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];

  s_hw_available = false;
  prv_ref_sha256(s_data, sizeof(s_data), expected);
  pbl_sha256(s_data, sizeof(s_data), digest);
  cl_assert_equal_m(digest, expected, sizeof(expected));
  cl_assert(s_hw_calls > 0);
}

void test_sha256_hw__short_runs_stay_in_software(void) {
  uint8_t expected[PBL_SHA256_DIGEST_SIZE];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];
  struct pbl_sha256_ctx ctx;

  prv_ref_sha256(s_data, 200, expected);
  pbl_sha256_init(&ctx);
  for (size_t i = 0; i < 200; i += 100) {
    pbl_sha256_update(&ctx, &s_data[i], 100);
  }
  pbl_sha256_final(&ctx, digest);
  cl_assert_equal_m(digest, expected, sizeof(expected));
  cl_assert_equal_i(s_hw_calls, 0);
}

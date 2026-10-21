/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/util/sha256.h>

static const uint32_t K[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t prv_rotr(uint32_t x, unsigned int n) {
  return (x >> n) | (x << (32U - n));
}

static void prv_transform(Sha256 *sha, const uint8_t *block) {
  uint32_t w[64];
  uint32_t s[8];

  for (unsigned int i = 0; i < 16U; i++) {
    w[i] = ((uint32_t)block[4U * i] << 24) | ((uint32_t)block[4U * i + 1U] << 16) |
           ((uint32_t)block[4U * i + 2U] << 8) | (uint32_t)block[4U * i + 3U];
  }
  for (unsigned int i = 16; i < 64U; i++) {
    uint32_t s0 = prv_rotr(w[i - 15U], 7) ^ prv_rotr(w[i - 15U], 18) ^ (w[i - 15U] >> 3);
    uint32_t s1 = prv_rotr(w[i - 2U], 17) ^ prv_rotr(w[i - 2U], 19) ^ (w[i - 2U] >> 10);
    w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
  }

  memcpy(s, sha->state, sizeof(s));
  for (unsigned int i = 0; i < 64U; i++) {
    uint32_t t1 = s[7] + (prv_rotr(s[4], 6) ^ prv_rotr(s[4], 11) ^ prv_rotr(s[4], 25)) +
                  ((s[4] & s[5]) ^ (~s[4] & s[6])) + K[i] + w[i];
    uint32_t t2 = (prv_rotr(s[0], 2) ^ prv_rotr(s[0], 13) ^ prv_rotr(s[0], 22)) +
                  ((s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]));
    memmove(&s[1], &s[0], 7U * sizeof(s[0]));
    s[4] += t1;
    s[0] = t1 + t2;
  }

  for (unsigned int i = 0; i < 8U; i++) {
    sha->state[i] += s[i];
  }
}

void sha256_init(Sha256 *sha) {
  static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };

  memcpy(sha->state, H0, sizeof(H0));
  sha->length = 0;
}

void sha256_update(Sha256 *sha, const void *data, size_t length) {
  const uint8_t *bytes = data;

  while (length > 0U) {
    size_t used = sha->length % sizeof(sha->block);
    size_t n = sizeof(sha->block) - used;
    if (n > length) {
      n = length;
    }
    memcpy(&sha->block[used], bytes, n);
    sha->length += n;
    bytes += n;
    length -= n;
    if ((sha->length % sizeof(sha->block)) == 0U) {
      prv_transform(sha, sha->block);
    }
  }
}

void sha256_final(Sha256 *sha, uint8_t digest[SHA256_DIGEST_SIZE]) {
  const uint64_t bits = sha->length * 8U;
  const uint8_t pad = 0x80;
  const uint8_t zero = 0;
  uint8_t be_bits[8];

  sha256_update(sha, &pad, 1);
  while ((sha->length % sizeof(sha->block)) != 56U) {
    sha256_update(sha, &zero, 1);
  }
  for (unsigned int i = 0; i < 8U; i++) {
    be_bits[i] = (uint8_t)(bits >> (56U - 8U * i));
  }
  sha256_update(sha, be_bits, sizeof(be_bits));

  for (unsigned int i = 0; i < 8U; i++) {
    digest[4U * i] = (uint8_t)(sha->state[i] >> 24);
    digest[4U * i + 1U] = (uint8_t)(sha->state[i] >> 16);
    digest[4U * i + 2U] = (uint8_t)(sha->state[i] >> 8);
    digest[4U * i + 3U] = (uint8_t)sha->state[i];
  }
}

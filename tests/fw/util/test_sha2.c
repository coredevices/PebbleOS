/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "util/sha2.h"

#include <string.h>

void test_sha2__initialize(void) {}
void test_sha2__cleanup(void) {}

// FIPS 180-2 known answer: SHA-256("abc")
void test_sha2__kat_abc(void) {
  static const uint8_t expected[SHA256_DIGEST_SIZE] = {
    0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
    0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad,
  };
  uint8_t out[SHA256_DIGEST_SIZE];
  sha256_hash((const uint8_t *)"abc", 3, out);
  cl_assert_equal_i(0, memcmp(out, expected, SHA256_DIGEST_SIZE));
}

// SHA-256("") empty input
void test_sha2__kat_empty(void) {
  static const uint8_t expected[SHA256_DIGEST_SIZE] = {
    0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
    0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55,
  };
  uint8_t out[SHA256_DIGEST_SIZE];
  sha256_hash((const uint8_t *)"", 0, out);
  cl_assert_equal_i(0, memcmp(out, expected, SHA256_DIGEST_SIZE));
}

// Streaming in two chunks must equal one-shot.
void test_sha2__streaming_matches_oneshot(void) {
  const char *msg = "the quick brown fox";
  uint8_t a[SHA256_DIGEST_SIZE], b[SHA256_DIGEST_SIZE];
  sha256_hash((const uint8_t *)msg, strlen(msg), a);

  SHA256Context ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, (const uint8_t *)msg, 4);
  sha256_update(&ctx, (const uint8_t *)msg + 4, strlen(msg) - 4);
  sha256_final(&ctx, b);
  cl_assert_equal_i(0, memcmp(a, b, SHA256_DIGEST_SIZE));
}

// FIPS 180-2 two-block vector: SHA-256 of 56-byte string spanning two 512-bit blocks.
// Reference: printf 'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq' | sha256sum
void test_sha2__kat_two_block(void) {
  static const uint8_t expected[SHA256_DIGEST_SIZE] = {
    0x24,0x8d,0x6a,0x61,0xd2,0x06,0x38,0xb8,0xe5,0xc0,0x26,0x93,0x0c,0x3e,0x60,0x39,
    0xa3,0x3c,0xe4,0x59,0x64,0xff,0x21,0x67,0xf6,0xec,0xed,0xd4,0x19,0xdb,0x06,0xc1,
  };
  uint8_t out[SHA256_DIGEST_SIZE];
  sha256_hash((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
  cl_assert_equal_i(0, memcmp(out, expected, SHA256_DIGEST_SIZE));
}

// Long input (200 bytes) forcing multiple mid-stream block flushes in sha256_update.
// Reference: python3 -c "import sys; sys.stdout.buffer.write(b'A'*200)" | sha256sum
void test_sha2__kat_long(void) {
  static const uint8_t expected[SHA256_DIGEST_SIZE] = {
    0x70,0xd3,0xbf,0x8b,0x0b,0x9d,0x83,0xa6,0x10,0x12,0xf3,0x5f,0xbf,0x46,0x0c,0x42,
    0x07,0x06,0x3f,0xe3,0x1b,0x4d,0x61,0x78,0x39,0x0f,0xe3,0xb7,0x21,0xcc,0x03,0xf7,
  };
  uint8_t buf[200];
  memset(buf, 'A', sizeof(buf));
  uint8_t out[SHA256_DIGEST_SIZE];
  sha256_hash(buf, sizeof(buf), out);
  cl_assert_equal_i(0, memcmp(out, expected, SHA256_DIGEST_SIZE));
}

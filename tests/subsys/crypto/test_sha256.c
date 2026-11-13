/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/crypto/sha256.h>

#include <clar.h>

static uint8_t s_data[1031];

static void prv_hex(const uint8_t digest[PBL_SHA256_DIGEST_SIZE], char *hex) {
  for (size_t i = 0; i < PBL_SHA256_DIGEST_SIZE; i++) {
    snprintf(&hex[2 * i], 3, "%02x", digest[i]);
  }
}

static void prv_check(const char *msg, const char *expected) {
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];
  char hex[2 * PBL_SHA256_DIGEST_SIZE + 1];

  pbl_sha256(msg, strlen(msg), digest);
  prv_hex(digest, hex);
  cl_assert_equal_s(hex, expected);
}

void test_sha256__initialize(void) {
  uint32_t x = 0x12345678;
  for (size_t i = 0; i < sizeof(s_data); i++) {
    x = x * 1103515245U + 12345U;
    s_data[i] = (uint8_t)(x >> 16);
  }
}

// FIPS 180-4 examples
void test_sha256__empty(void) {
  prv_check("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

void test_sha256__one_block(void) {
  prv_check("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void test_sha256__two_blocks(void) {
  prv_check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

void test_sha256__896_bits(void) {
  prv_check(
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqr"
      "lmnopqrsmnopqrstnopqrstu",
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

void test_sha256__million_a(void) {
  struct pbl_sha256_ctx ctx;
  uint8_t chunk[1000];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];
  char hex[2 * PBL_SHA256_DIGEST_SIZE + 1];

  memset(chunk, 'a', sizeof(chunk));
  pbl_sha256_init(&ctx);
  for (int i = 0; i < 1000; i++) {
    pbl_sha256_update(&ctx, chunk, sizeof(chunk));
  }
  pbl_sha256_final(&ctx, digest);
  prv_hex(digest, hex);
  cl_assert_equal_s(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

void test_sha256__padding_boundaries(void) {
  // 55 to 59 bytes, around the length that spills the padding into another block
  static const char *const expected[] = {
    "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
    "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
    "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6",
    "d5c039b748aa64665782974ec3dc3025c042edf54dcdc2b5de31385b094cb678",
    "111bb261277afd65f0744b247cd3e47d386d71563d0ed995517807d5ebd4fba3",
  };
  char msg[66];

  memset(msg, 'a', sizeof(msg));
  for (size_t i = 0; i < 5; i++) {
    msg[55 + i] = '\0';
    prv_check(msg, expected[i]);
    msg[55 + i] = 'a';
  }
}

void test_sha256__streaming(void) {
  uint8_t whole[PBL_SHA256_DIGEST_SIZE];

  pbl_sha256(s_data, sizeof(s_data), whole);
  for (size_t split = 0; split <= 130; split++) {
    struct pbl_sha256_ctx ctx;
    uint8_t digest[PBL_SHA256_DIGEST_SIZE];

    pbl_sha256_init(&ctx);
    pbl_sha256_update(&ctx, s_data, split);
    pbl_sha256_update(&ctx, &s_data[split], sizeof(s_data) - split);
    pbl_sha256_final(&ctx, digest);
    cl_assert_equal_m(digest, whole, sizeof(whole));
  }
}

void test_sha256__byte_at_a_time(void) {
  struct pbl_sha256_ctx ctx;
  uint8_t whole[PBL_SHA256_DIGEST_SIZE];
  uint8_t digest[PBL_SHA256_DIGEST_SIZE];

  pbl_sha256(s_data, sizeof(s_data), whole);
  pbl_sha256_init(&ctx);
  for (size_t i = 0; i < sizeof(s_data); i++) {
    pbl_sha256_update(&ctx, &s_data[i], 1);
  }
  pbl_sha256_final(&ctx, digest);
  cl_assert_equal_m(digest, whole, sizeof(whole));
}

void test_sha256__blocks_generic(void) {
  static const uint32_t expected[8] = {
    0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad,
  };
  uint32_t state[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  uint8_t block[PBL_SHA256_BLOCK_SIZE] = {'a', 'b', 'c', 0x80};

  block[PBL_SHA256_BLOCK_SIZE - 1] = 24;
  pbl_sha256_blocks_generic(state, block, 1);
  cl_assert_equal_m(state, expected, sizeof(expected));
}

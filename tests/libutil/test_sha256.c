/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/util/sha256.h>

#include <clar.h>

static void prv_check(const void *data, size_t length, const char *expected_hex) {
  Sha256 sha;
  uint8_t digest[SHA256_DIGEST_SIZE];
  char hex[2 * SHA256_DIGEST_SIZE + 1];

  sha256_init(&sha);
  sha256_update(&sha, data, length);
  sha256_final(&sha, digest);
  for (size_t i = 0; i < sizeof(digest); i++) {
    snprintf(&hex[2 * i], 3, "%02x", digest[i]);
  }
  cl_assert_equal_s(hex, expected_hex);
}

void test_sha256__empty(void) {
  prv_check("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

void test_sha256__abc(void) {
  prv_check("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void test_sha256__two_blocks(void) {
  const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  prv_check(msg, strlen(msg), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

void test_sha256__incremental(void) {
  static uint8_t data[1000];
  memset(data, 'a', sizeof(data));

  Sha256 sha;
  uint8_t digest[SHA256_DIGEST_SIZE];
  sha256_init(&sha);
  for (int i = 0; i < 1000; i++) {
    sha256_update(&sha, data, sizeof(data));
  }
  sha256_final(&sha, digest);
  // SHA-256 of one million 'a'
  const uint8_t expected[4] = {0xcd, 0xc7, 0x6e, 0x5c};
  cl_assert_equal_m(digest, expected, sizeof(expected));
}

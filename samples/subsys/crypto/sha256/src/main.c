/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/crypto/sha256.h>
#include <pbl/util/math.h>

#define DATA_LEN 1000U

static uint8_t s_data[DATA_LEN];

static void prv_print(const char *what, const uint8_t digest[PBL_SHA256_DIGEST_SIZE]) {
  printf("sha256: %s: ", what);
  for (size_t i = 0; i < PBL_SHA256_DIGEST_SIZE; i++) {
    printf("%02x", digest[i]);
  }
  printf("\n");
}

int main(void) {
  static const char two_blocks[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  uint8_t digest[PBL_SHA256_DIGEST_SIZE], chunked[PBL_SHA256_DIGEST_SIZE];
  struct pbl_sha256_ctx ctx;
  size_t off = 0, chunk = 1;

  pbl_sha256("abc", 3, digest);
  prv_print("\"abc\"", digest);

  pbl_sha256(two_blocks, strlen(two_blocks), digest);
  prv_print("56 bytes, two blocks", digest);

  for (size_t i = 0; i < DATA_LEN; i++) {
    s_data[i] = (uint8_t)(i * 7U);
  }
  pbl_sha256(s_data, DATA_LEN, digest);

  /* Partial blocks are kept in the context until the next update completes them. */
  pbl_sha256_init(&ctx);
  while (off < DATA_LEN) {
    size_t len = PBL_MIN(chunk, DATA_LEN - off);

    pbl_sha256_update(&ctx, &s_data[off], len);
    off += len;
    chunk = chunk * 7U % 97U + 1U;
  }
  pbl_sha256_final(&ctx, chunked);
  printf("sha256: %u bytes in uneven chunks, the same as in one go: %s\n", DATA_LEN,
         memcmp(digest, chunked, sizeof(digest)) == 0 ? "yes" : "no");

  printf("Sample done\n");

  return 0;
}

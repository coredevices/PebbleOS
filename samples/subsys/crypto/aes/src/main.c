/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/crypto/aes.h>

#define MSG_LEN (3U * PBL_AES_BLOCK_SIZE)

/* The example vectors of FIPS 197, appendix C. */
static const uint8_t s_plain[PBL_AES_BLOCK_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static struct pbl_aes_ctx s_ctx;

static void prv_print(const char *what, const uint8_t block[PBL_AES_BLOCK_SIZE]) {
  printf("aes: %s: ", what);
  for (size_t i = 0; i < PBL_AES_BLOCK_SIZE; i++) {
    printf("%02x", block[i]);
  }
  printf("\n");
}

int main(void) {
  uint8_t key[PBL_AES_KEYSIZE_256], block[PBL_AES_BLOCK_SIZE], back[PBL_AES_BLOCK_SIZE];
  uint8_t msg[MSG_LEN], enc[MSG_LEN], dec[MSG_LEN];
  int ret;

  for (size_t i = 0; i < sizeof(key); i++) {
    key[i] = (uint8_t)i;
  }

  /* The key schedule is computed once, then used for any number of blocks. */
  ret = pbl_aes_expandkey(&s_ctx, key, PBL_AES_KEYSIZE_128);
  if (ret < 0) {
    fprintf(stderr, "aes: key expansion failed (%d)\n", ret);
    return ret;
  }
  pbl_aes_encrypt(&s_ctx, block, s_plain);
  prv_print("AES-128", block);

  ret = pbl_aes_expandkey(&s_ctx, key, PBL_AES_KEYSIZE_256);
  if (ret < 0) {
    fprintf(stderr, "aes: key expansion failed (%d)\n", ret);
    return ret;
  }
  pbl_aes_encrypt(&s_ctx, block, s_plain);
  prv_print("AES-256", block);

  pbl_aes_decrypt(&s_ctx, back, block);
  printf("aes: decrypted back to the plaintext: %s\n",
         memcmp(back, s_plain, sizeof(back)) == 0 ? "yes" : "no");

  /* ECB encrypts each block on its own: the first and last blocks are the same. */
  memcpy(msg, "Sixteen bytes...and sixteen moreSixteen bytes...", MSG_LEN);
  ret = pbl_aes_ecb_encrypt(&s_ctx, enc, msg, MSG_LEN);
  if (ret == 0) {
    ret = pbl_aes_ecb_decrypt(&s_ctx, dec, enc, MSG_LEN);
  }
  if (ret < 0) {
    fprintf(stderr, "aes: ECB failed (%d)\n", ret);
    return ret;
  }
  printf("aes: %u bytes in ECB mode, decrypted back: %s\n", MSG_LEN,
         memcmp(dec, msg, MSG_LEN) == 0 ? "yes" : "no");
  printf("aes: equal blocks encrypt to equal blocks in ECB mode: %s\n",
         memcmp(enc, &enc[2 * PBL_AES_BLOCK_SIZE], PBL_AES_BLOCK_SIZE) == 0 ? "yes" : "no");

  ret = pbl_aes_ecb_encrypt(&s_ctx, enc, msg, 20);
  printf("aes: 20 bytes, not whole blocks: %s\n", ret == -EINVAL ? "-EINVAL" : "accepted");

  printf("Sample done\n");

  return 0;
}

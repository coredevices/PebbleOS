/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/crc/crc.h>

static const char s_check[] = "123456789";

int main(void) {
  uint8_t frame[64];
  size_t len = strlen(s_check);
  uint32_t crc;

  printf("crc: CRC-8 of \"%s\": 0x%02x\n", s_check, pbl_crc8(0, s_check, len));

  crc = pbl_crc32(0, s_check, len);
  printf("crc: CRC-32 of \"%s\": 0x%08x\n", s_check, (unsigned int)crc);

  /* The running value carries over, so data can come in pieces. */
  crc = pbl_crc32(0, s_check, 4);
  crc = pbl_crc32(crc, &s_check[4], len - 4);
  printf("crc: CRC-32 in chunks: 0x%08x\n", (unsigned int)crc);

  /* A receiver checks a frame that carries its CRC-32, little-endian, without splitting it. */
  memcpy(frame, s_check, len);
  for (int i = 0; i < 4; i++) {
    frame[len + i] = (uint8_t)(crc >> (8 * i));
  }
  printf("crc: message with its CRC-32 appended is intact: %s\n",
         pbl_crc32(0, frame, len + 4) == PBL_CRC32_RESIDUE ? "yes" : "no");

  frame[2] ^= 0x10;
  printf("crc: after flipping a bit: %s\n",
         pbl_crc32(0, frame, len + 4) == PBL_CRC32_RESIDUE ? "yes" : "no");

  /* The checksum of older on-flash formats keeps partial words between updates. */
  struct pbl_crc32_legacy ctx;
  uint32_t legacy = pbl_crc32_legacy(s_check, len);

  pbl_crc32_legacy_init(&ctx);
  pbl_crc32_legacy_update(&ctx, s_check, 3);
  pbl_crc32_legacy_update(&ctx, &s_check[3], len - 3);
  printf("crc: legacy checksum: 0x%08x, in chunks: %s\n", (unsigned int)legacy,
         pbl_crc32_legacy_finish(&ctx) == legacy ? "the same" : "different");

  printf("Sample done\n");

  return 0;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/rng.h>
#include <pbl/kernel/compiler.h>

#define WORDS       4
#define STATS_WORDS 1000

int main(void) {
  uint32_t val, prev = 0U;
  unsigned int ones = 0U, repeats = 0U;

  for (int i = 0; i < WORDS; i++) {
    if (rng_rand(&val) != 0) {
      fprintf(stderr, "rng: generation failed\n");
      return -EIO;
    }

    printf("rng: 0x%08x\n", (unsigned int)val);
  }

  /* A good source sets about half of the bits, and does not repeat itself. */
  for (int i = 0; i < STATS_WORDS; i++) {
    if (rng_rand(&val) != 0) {
      fprintf(stderr, "rng: generation failed\n");
      return -EIO;
    }

    if (i > 0 && val == prev) {
      repeats++;
    }
    ones += PBL_POPCOUNT(val);
    prev = val;
  }

  printf("rng: %d words, %u repeated, %u per mille of the bits set\n", STATS_WORDS, repeats,
         ones * 1000U / (STATS_WORDS * 32U));

  printf("Sample done\n");

  return 0;
}

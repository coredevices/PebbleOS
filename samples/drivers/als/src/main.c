/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/als.h>
#include <pbl/kernel/delay.h>

#define READS     5
#define PERIOD_MS 1000

int main(void) {
  int ret = 0;

  printf("als: levels 0 to %u, lux: %s\n", (unsigned int)PBL_ALS_LEVEL_MAX,
         pbl_als_has_lux(ALS) ? "yes" : "no");

  /* Priming lets sensors that sample continuously start before the first read. */
  pbl_als_prime(ALS);

  for (int i = 0; i < READS; i++) {
    uint32_t level;

    pbl_sleep_ms(PERIOD_MS);

    ret = pbl_als_read(ALS, &level);
    if (ret != 0) {
      fprintf(stderr, "als: read failed (%d)\n", ret);
      break;
    }

    /* Uncalibrated sensors return the level unchanged. */
    printf("als: level %u, %u %s\n", (unsigned int)level, (unsigned int)pbl_als_to_lux(ALS, level),
           pbl_als_has_lux(ALS) ? "lux" : "counts");
  }

  pbl_als_release(ALS);

  if (ret != 0) {
    return ret;
  }

  printf("Sample done\n");

  return 0;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pbl/drivers/temp.h>
#include <pbl/kernel/delay.h>

#define READS 5

int main(void) {
  for (int i = 0; i < READS; i++) {
    int32_t millicelsius;
    int ret;

    /* DIE_TEMP is the board's die temperature sensor, whatever drives it. */
    ret = pbl_temp_read(DIE_TEMP, &millicelsius);
    if (ret != 0) {
      fprintf(stderr, "temp: read failed (%d)\n", ret);
      return ret;
    }

    printf("temp: %s%d.%d C\n", millicelsius < 0 ? "-" : "", (int)(abs(millicelsius) / 1000),
           (int)(abs(millicelsius) % 1000 / 100));

    pbl_sleep_ms(1000);
  }

  printf("Sample done\n");

  return 0;
}

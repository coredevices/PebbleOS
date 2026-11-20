/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/pressure.h>
#include <pbl/kernel/delay.h>

#define READS 5

int main(void) {
  for (int i = 0; i < READS; i++) {
    uint32_t pa;
    int ret;

    /* Each read runs one conversion; the sensor sleeps in between. */
    ret = pbl_pressure_read(PRESSURE, &pa);
    if (ret != 0) {
      fprintf(stderr, "pressure: read failed (%d)\n", ret);
      return ret;
    }

    printf("pressure: %d.%02d hPa\n", (int)(pa / 100U), (int)(pa % 100U));

    pbl_sleep_ms(1000);
  }

  printf("Sample done\n");

  return 0;
}

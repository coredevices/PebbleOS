/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/delay.h>

#define READS 5

int main(void) {
  for (int i = 0; i < READS; i++) {
    int32_t mpa;
    int ret;

    /* Each read runs one conversion; the sensor sleeps in between. */
    ret = pbl_sensor_read(PRESSURE, PBL_SENSOR_PRESSURE, &mpa, nullptr);
    if (ret != 0) {
      fprintf(stderr, "pressure: read failed (%d)\n", ret);
      return ret;
    }

    printf("pressure: %d.%02d hPa\n", (int)(mpa / 100000), (int)(mpa / 1000 % 100));

    pbl_sleep_ms(1000);
  }

  printf("Sample done\n");

  return 0;
}

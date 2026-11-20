/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/drivers/led.h>
#include <pbl/kernel/delay.h>

#define BLINKS    5
#define PERIOD_MS 1000

int main(void) {
  int ret;

  for (int i = 0; i < BLINKS; i++) {
    ret = pbl_led_set(STATUS_LED, PBL_LED_BRIGHTNESS_MAX, 0xffffffU);
    if (ret != 0) {
      fprintf(stderr, "led: on failed (%d)\n", ret);
      return ret;
    }
    printf("led: on\n");
    pbl_sleep_ms(PERIOD_MS / 2);

    ret = pbl_led_set(STATUS_LED, 0U, 0U);
    if (ret != 0) {
      fprintf(stderr, "led: off failed (%d)\n", ret);
      return ret;
    }
    printf("led: off\n");
    pbl_sleep_ms(PERIOD_MS / 2);
  }

  printf("led: blinked %d times\n", BLINKS);
  printf("Sample done\n");

  return 0;
}

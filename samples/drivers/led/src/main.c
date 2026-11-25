/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/led.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/size.h>

#define BLINKS    5
#define PERIOD_MS 1000

#ifdef CONFIG_LED_HAS_COLOR
static const struct {
  const char *name;
  uint32_t rgb;
} s_colors[] = {
  {"red", 0xff0000U},
  {"green", 0x00ff00U},
  {"blue", 0x0000ffU},
};
#endif

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

#ifdef CONFIG_LED_HAS_COLOR
  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_colors); i++) {
    ret = pbl_led_set(STATUS_LED, PBL_LED_BRIGHTNESS_MAX, s_colors[i].rgb);
    if (ret != 0) {
      fprintf(stderr, "led: %s failed (%d)\n", s_colors[i].name, ret);
      return ret;
    }
    printf("led: %s\n", s_colors[i].name);
    pbl_sleep_ms(PERIOD_MS);
  }

  (void)pbl_led_set(STATUS_LED, 0U, 0U);
#endif
  printf("Sample done\n");

  return 0;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/backlight.h>
#include <pbl/kernel/delay.h>

#define STEP    5
#define STEP_MS 50

static int prv_set(uint8_t brightness) {
  int ret;

  ret = pbl_backlight_set_brightness(BACKLIGHT, brightness);
  if (ret != 0) {
    fprintf(stderr, "backlight: set to %d failed (%d)\n", brightness, ret);
  }

  return ret;
}

static int prv_ramp(int from, int to) {
  int step = from < to ? STEP : -STEP;
  int ret;

  for (int b = from; b != to + step; b += step) {
    ret = prv_set((uint8_t)b);
    if (ret != 0) {
      return ret;
    }
    pbl_sleep_ms(STEP_MS);
  }

  return 0;
}

int main(void) {
  static const uint32_t colors[] = {
    BACKLIGHT_COLOR_RED, BACKLIGHT_COLOR_GREEN, BACKLIGHT_COLOR_BLUE, BACKLIGHT_COLOR_WHITE
  };
  int ret;

  /* Brightness goes from 0 (off) to PBL_BACKLIGHT_BRIGHTNESS_MAX. */
  ret = prv_ramp(0, PBL_BACKLIGHT_BRIGHTNESS_MAX);
  if (ret != 0) {
    return ret;
  }
  printf("backlight: ramped up to %d\n", PBL_BACKLIGHT_BRIGHTNESS_MAX);

  ret = prv_ramp(PBL_BACKLIGHT_BRIGHTNESS_MAX, 0);
  if (ret != 0) {
    return ret;
  }
  printf("backlight: ramped down to 0\n");

  ret = prv_set(PBL_BACKLIGHT_BRIGHTNESS_MAX);
  if (ret != 0) {
    return ret;
  }

  /* Backlights without color refuse it with -ENOTSUP. */
  for (unsigned int i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
    ret = pbl_backlight_set_color(BACKLIGHT, colors[i]);
    if (ret == -ENOTSUP) {
      printf("backlight: no color\n");
      break;
    } else if (ret != 0) {
      fprintf(stderr, "backlight: color failed (%d)\n", ret);
      return ret;
    }
    printf("backlight: color 0x%x\n", (unsigned int)pbl_backlight_get_color(BACKLIGHT));
    pbl_sleep_ms(500);
  }

  ret = prv_set(0);
  if (ret != 0) {
    return ret;
  }
  printf("backlight: off\n");

  printf("Sample done\n");

  return 0;
}

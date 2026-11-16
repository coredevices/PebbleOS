/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/vibe.h>
#include <pbl/kernel/delay.h>

#define PULSES   3
#define PULSE_MS 150
#define PAUSE_MS 350
#define BRAKE_MS 40

static int prv_pulse(int8_t strength, int8_t braking) {
  int ret;

  ret = pbl_vibe_start(VIBE, strength);
  if (ret != 0) {
    fprintf(stderr, "vibe: start failed (%d)\n", ret);
    return ret;
  }
  pbl_sleep_ms(PULSE_MS);

  /* Driving the motor against itself for a moment stops it sooner. */
  if (braking != VIBE_STRENGTH_OFF) {
    ret = pbl_vibe_start(VIBE, braking);
    if (ret != 0) {
      fprintf(stderr, "vibe: brake failed (%d)\n", ret);
      return ret;
    }
    pbl_sleep_ms(BRAKE_MS);
  }

  ret = pbl_vibe_stop(VIBE);
  if (ret != 0) {
    fprintf(stderr, "vibe: stop failed (%d)\n", ret);
  }

  return ret;
}

int main(void) {
  int8_t braking = pbl_vibe_get_braking_strength(VIBE);
  int ret;

  for (int i = 0; i < PULSES; i++) {
    ret = prv_pulse(VIBE_STRENGTH_MAX, VIBE_STRENGTH_OFF);
    if (ret != 0) {
      return ret;
    }
    printf("vibe: pulse %d\n", i + 1);
    pbl_sleep_ms(PAUSE_MS);
  }

  if (braking == VIBE_STRENGTH_OFF) {
    printf("vibe: no braking\n");
  } else {
    for (int i = 0; i < PULSES; i++) {
      ret = prv_pulse(VIBE_STRENGTH_MAX, braking);
      if (ret != 0) {
        return ret;
      }
      printf("vibe: braked pulse %d, braking at %d\n", i + 1, braking);
      pbl_sleep_ms(PAUSE_MS);
    }
  }

  printf("Sample done\n");

  return 0;
}

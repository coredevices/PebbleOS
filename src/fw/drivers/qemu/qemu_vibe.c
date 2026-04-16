/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/vibe.h"

#include "drivers/qemu/qemu_serial.h"
#include "console/prompt.h"

#include <stdlib.h>

static bool s_vibe_on;

void vibe_init(void) {
  s_vibe_on = false;
}

void vibe_set_strength(int8_t strength) {
  (void)strength;
}

void vibe_ctl(bool on) {
  if (s_vibe_on == on) {
    return;
  }
  s_vibe_on = on;

  // Notify QEMU host of vibration state change
  QemuProtocolVibrationNotificationHeader notification = {
    .on = on ? 1 : 0,
  };
  qemu_serial_send(QemuProtocol_Vibration,
                   (const uint8_t *)&notification,
                   sizeof(notification));
}

void vibe_force_off(void) {
  vibe_ctl(false);
}

int8_t vibe_get_braking_strength(void) {
  return VIBE_STRENGTH_OFF;
}

status_t vibe_calibrate(void) {
  return E_INVALID_OPERATION;
}

void command_vibe_ctl(const char *arg) {
  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}

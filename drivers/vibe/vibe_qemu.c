/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>

#include <comm/qemu/serial.h>

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
  qemu_serial_send(QemuProtocol_Vibration, (const uint8_t *)&notification, sizeof(notification));
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

uint8_t vibe_get_calibration(void) {
  return 0xFF;
}

void vibe_apply_calibration(uint8_t cali) {
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_vibe(const struct pbl_shell *sh, size_t argc, char **argv) {
  long strength;
  if (pbl_shell_strtol(argv[1], &strength) != 0 || strength < 0 || strength > VIBE_STRENGTH_MAX) {
    pbl_shell_error(sh, "invalid argument '%s'", argv[1]);
    return -EINVAL;
  }

  vibe_set_strength((int8_t)strength);
  vibe_ctl(strength != 0);
  return 0;
}

PBL_SHELL_CMD_ARG_REGISTER(vibe, nullptr, "Vibrate at <strength 0-100>", prv_cmd_vibe, 2, 0);
#endif

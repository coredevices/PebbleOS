/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/qemu.h>

#include <comm/qemu/serial.h>

static bool s_vibe_on;

static int prv_set(bool on) {
  if (s_vibe_on == on) {
    return 0;
  }

  s_vibe_on = on;

  QemuProtocolVibrationNotificationHeader notification = {
    .on = on ? 1 : 0,
  };
  qemu_serial_send(QemuProtocol_Vibration, (const uint8_t *)&notification, sizeof(notification));

  return 0;
}

static int prv_init(const struct pbl_vibe *vibe) {
  s_vibe_on = false;

  return 0;
}

static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  return prv_set(true);
}

static int prv_stop(const struct pbl_vibe *vibe) {
  return prv_set(false);
}

static const struct pbl_vibe_ops s_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
};

PBL_VIBE_STATE_DEFINE(pbl_vibe_qemu);
const struct pbl_vibe pbl_vibe_qemu =
    PBL_VIBE_INIT(pbl_vibe_qemu, "vibe", nullptr, &s_ops, VIBE_STRENGTH_OFF);
PBL_DEVICE_REGISTER(pbl_vibe_qemu, &pbl_vibe_qemu.dev);

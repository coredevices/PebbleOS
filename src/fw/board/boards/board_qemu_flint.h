/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define BT_VENDOR_ID 0x0000
#define BT_VENDOR_NAME "QEMU"

extern UARTDevice * const DBG_UART;
extern UARTDevice * const QEMU_UART;
extern DisplayDevice *const DISPLAY;
extern const BoardConfigActuator BOARD_CONFIG_VIBE;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const BoardConfigButton BOARD_CONFIG_BUTTON;

static const BoardConfigBacklight BOARD_CONFIG_BACKLIGHT = {
  .options = 0,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
    .default_motion_sensitivity = 85U,
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  .mag_config = {{0}},
};

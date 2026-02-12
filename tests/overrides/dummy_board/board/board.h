/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum {
  CC2564A = 0,
  CC2564B,
} BluetoothController;

typedef struct {
  const uint8_t backlight_on_percent; // percent of max possible brightness
  const uint32_t ambient_light_dark_threshold;
} BoardConfig;

typedef struct {
  const BluetoothController controller;
} BoardConfigBTCommon;

typedef struct {
  //! Percentage for watch only mode
  const uint8_t low_power_threshold;

  //! Approximate hours of battery life
  const uint8_t battery_capacity_hours;
} BoardConfigPower;

static const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 100,
};

static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
  .controller = CC2564A,
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 5,
  .battery_capacity_hours = 144,
};

typedef struct {
  uint8_t default_motion_sensitivity;
} AccelConfig;

typedef struct {
  const AccelConfig accel_config;
} BoardConfigAccel;

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = { .default_motion_sensitivity = 0 },
};

typedef const struct MicDevice MicDevice;
static MicDevice * const MIC = (void *)0;

typedef const struct HRMDevice HRMDevice;
static HRMDevice * const HRM = (void *)0;

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define PLATFORM_TINTIN 1

typedef struct {
  const uint8_t backlight_on_percent; // percent of max possible brightness
} BoardConfig;

typedef struct {
} BoardConfigBTCommon;

typedef struct {
  const uint8_t low_power_threshold;
} BoardConfigPower;

static const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 100,
};

static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 5
};

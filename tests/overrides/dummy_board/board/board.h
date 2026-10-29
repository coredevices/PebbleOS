/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct {
  const uint8_t backlight_on_percent; // percent of max possible brightness
  const uint32_t ambient_light_dark_threshold;
} BoardConfig;

typedef struct {
} BoardConfigBTCommon;

static const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 100,
};

static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {};

typedef const struct MicDevice MicDevice;
static MicDevice *const MIC = (void *)0;

typedef const struct HRMDevice HRMDevice;
static HRMDevice *const HRM = (void *)0;

static const struct AudioDevice *const AUDIO = (void *)0;

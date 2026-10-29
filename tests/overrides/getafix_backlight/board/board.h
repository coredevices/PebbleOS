/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Keep this minimal board override aligned with dummy_board, but use Getafix's
// hardware backlight ceiling for scaling tests.
#include <stdint.h>

typedef struct {
  const uint8_t backlight_on_percent;
  const uint32_t ambient_light_dark_threshold;
  const uint32_t ambient_k_delta_threshold;
} BoardConfig;

typedef struct {
} BoardConfigBTCommon;

static const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 25,
};

static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {};

typedef const struct MicDevice MicDevice;
static MicDevice *const MIC = (void *)0;

typedef const struct HRMDevice HRMDevice;
static HRMDevice *const HRM = (void *)0;

static const struct AudioDevice *const AUDIO = (void *)0;

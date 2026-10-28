/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include <stdint.h>

#include <pbl/drivers/button_id.h>
#include <pbl/input/input.h>

typedef struct {
  uint8_t backlight_on_percent;
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  uint32_t backlight_default_color;
#endif
} BoardConfig;

typedef struct {
  const uint8_t low_power_threshold;
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef struct {
  uint8_t default_motion_sensitivity;
} BoardConfigAccel;

typedef const struct UARTDevice UARTDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct AudioDevice AudioDevice;

typedef struct PosixDisplayDevice {
  uint16_t width;
  uint16_t height;
} PosixDisplayDevice;
typedef const PosixDisplayDevice DisplayDevice;

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"

/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// clang-format off
#include "display.h"

#include <stdint.h>

#include <pbl/drivers/button_id.h>
#include <pbl/input/input.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/pwm.h>

#include <hal/nrf_gpio.h>
// clang-format on

typedef struct {
  // Ambient Light Configuration
  /////////////////////////////////////////////////////////////////////////////
  const uint32_t ambient_light_dark_threshold;
  const uint32_t ambient_k_delta_threshold;
  const struct pbl_gpio photo_en;

  const uint8_t backlight_on_percent; // percent of max possible brightness
} BoardConfig;

// Power Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  //! Percentage for watch only mode
  const uint8_t low_power_threshold;

  //! Approximate hours of battery life
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef struct {
  uint8_t default_motion_sensitivity;
} BoardConfigAccel;

struct pbl_uart;
typedef const struct HRMDevice HRMDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;

void board_early_init(void);
void board_init(void);

// clang-format off
#include <pbl/drivers/i2c/nrf5.h>
#include "board_definitions.h"
// clang-format on

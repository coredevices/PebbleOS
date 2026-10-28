/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include <stdint.h>

#include <pbl/drivers/button_id.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/input/input.h>

#include <bf0_hal_pinmux.h>

typedef struct {
  uint8_t backlight_on_percent;
  // ambient light config
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  // Default RGB backlight color (packed 0x00RRGGBB), applied when no app
  // override is set. User-preference overrides this via backlight_set_color().
  uint32_t backlight_default_color;
#endif
} BoardConfig;

typedef struct {
  //! Percentage for watch only mode
  const uint8_t low_power_threshold;
  //! Approximate hours of battery life
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef struct {
  uint8_t default_motion_sensitivity;
} BoardConfigAccel;

#include <pbl/drivers/display/sf32lb/display_jdi.h>
#include <pbl/drivers/flash/qspi_flash_definitions.h>
#include <pbl/drivers/mic/sf32lb52/pdm_definitions.h>
#include <pbl/drivers/qspi_definitions.h>
#include <pbl/drivers/speaker/sf32lb52/audio_definitions.h>
#include <pbl/drivers/uart/sf32lb.h>

typedef const struct UARTDevice UARTDevice;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct AudioDevice AudioDevice;

#include <pbl/drivers/i2c/sf32lb.h>

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"

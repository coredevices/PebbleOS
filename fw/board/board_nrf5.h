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
#include <nrfx_pdm.h>
// clang-format on

struct pbl_uart;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct AudioDevice AudioDevice;

void board_early_init(void);
void board_init(void);

// clang-format off
#include <pbl/drivers/i2c/nrf5.h>
#include "board_definitions.h"
// clang-format on

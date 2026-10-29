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

// clang-format off
#include <pbl/drivers/flash/qspi_flash_definitions.h>
#include <pbl/drivers/mic/sf32lb52/pdm_definitions.h>
#include <pbl/drivers/qspi_definitions.h>
#include <pbl/drivers/speaker/sf32lb52/audio_definitions.h>
// clang-format on

struct pbl_uart;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct AudioDevice AudioDevice;

#include <pbl/drivers/i2c/sf32lb.h>

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"

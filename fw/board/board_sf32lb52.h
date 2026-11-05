/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/input/input.h>

#include <bf0_hal_pinmux.h>

struct pbl_uart;
typedef const struct HRMDevice HRMDevice;

#include <pbl/drivers/i2c/sf32lb.h>

void board_early_init(void);
void board_init(void);

#ifdef CONFIG_BOARD_OBELIX
#include <board/boards/board_obelix.h>
#elifdef CONFIG_BOARD_GETAFIX
#include <board/boards/board_getafix.h>
#else
#error "Unknown board definition"
#endif

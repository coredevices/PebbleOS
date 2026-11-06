/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/input/input.h>

#include <cmsis_core.h>
#include <hardware/qemu_memorymap.h>

// Forward-declare device types
struct pbl_uart;
typedef const struct HRMDevice HRMDevice;

void board_early_init(void);
void board_init(void);

// QEMU doesn't have newlib-nano sniprintf; use standard snprintf
#include <stdio.h>
#define sniprintf snprintf

#ifdef CONFIG_BOARD_QEMU_EMERY
#include <board/boards/board_qemu_emery.h>
#elifdef CONFIG_BOARD_QEMU_FLINT
#include <board/boards/board_qemu_flint.h>
#elifdef CONFIG_BOARD_QEMU_GABBRO
#include <board/boards/board_qemu_gabbro.h>
#else
#error "Unknown board definition"
#endif

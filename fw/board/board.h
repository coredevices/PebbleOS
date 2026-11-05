/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Console port.
extern const struct pbl_uart *const CONSOLE_UART;

#if defined(CONFIG_QEMU)
#include "board_qemu.h"
#elifdef CONFIG_SOC_NRF52
#include "board_nrf5.h"
#elifdef CONFIG_SOC_SF32LB52
#include "board_sf32lb52.h"
#elifdef CONFIG_SOC_POSIX
#include "board_posix.h"
#elif !defined(SDK) && !defined(UNITTEST)
#error "Unknown or missing MICRO_FAMILY_* define"
#endif

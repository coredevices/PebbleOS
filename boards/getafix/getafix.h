/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/i2c/sf32lb.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/input/input.h>
#include <pbl/services/imu/units.h>

#ifdef CONFIG_REMOTEPROC_SF32LB52
extern const struct pbl_rproc_sf32lb52 *const LCPU;
#endif // CONFIG_REMOTEPROC_SF32LB52
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern const struct pbl_uart *const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

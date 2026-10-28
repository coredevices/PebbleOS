/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/imu/units.h>

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

#ifdef CONFIG_REMOTEPROC_SF32LB52
extern const struct pbl_rproc_sf32lb52 *const LCPU;
#endif // CONFIG_REMOTEPROC_SF32LB52
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern const struct pbl_uart *const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

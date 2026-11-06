/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef CONFIG_REMOTEPROC_SF32LB52
extern const struct pbl_rproc_sf32lb52 *const LCPU;
#endif // CONFIG_REMOTEPROC_SF32LB52
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern const struct pbl_uart *const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern HRMDevice *const HRM;

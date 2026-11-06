/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

extern const struct pbl_uart *const QEMU_UART;
#ifdef CONFIG_BT_HCI_UART
extern const struct pbl_uart *const BT_HCI_UART;
#endif
extern HRMDevice *const HRM;

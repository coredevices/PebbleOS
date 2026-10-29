/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "QEMU"

extern const struct pbl_uart *const DBG_UART;
extern const struct pbl_uart *const QEMU_UART;
#ifdef CONFIG_BT_HCI_UART
extern const struct pbl_uart *const BT_HCI_UART;
#endif
extern MicDevice *const MIC;
extern HRMDevice *const HRM;
extern const BoardConfig BOARD_CONFIG;

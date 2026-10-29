/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/imu/units.h>

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

extern const struct pbl_uart *const DBG_UART;
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern const struct pbl_uart *const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern const MicDevice *MIC;
extern HRMDevice *const HRM;
extern const AudioDevice *AUDIO;

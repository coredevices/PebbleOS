/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_VREGUSB_TASKS_START_OFFSET         0x0000
#define NRF54L_VREGUSB_EVENTS_VBUSDETECTED_OFFSET 0x0104
#define NRF54L_VREGUSB_EVENTS_VBUSREMOVED_OFFSET  0x0110
#define NRF54L_VREGUSB_INTENSET_OFFSET            0x0304
#define NRF54L_VREGUSB_UNDOC_STATUS_OFFSET        0x0400

/* Register bit definitions */

#define VREGUSB_INT_VBUSDETECTED PBL_BIT(1)
#define VREGUSB_INT_VBUSREMOVED  PBL_BIT(4)

/* Undocumented: VBUS present */
#define VREGUSB_STATUS_VBUS PBL_BIT(2)

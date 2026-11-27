/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_USBHS_TASKS_START_OFFSET        0x0000
#define NRF54L_USBHS_ENABLE_OFFSET             0x0400
#define NRF54L_USBHS_PHY_INPUTOVERRIDE_OFFSET  0x0458
#define NRF54L_USBHS_PHY_OVERRIDEVALUES_OFFSET 0x045c

/* Register bit definitions */

#define USBHS_ENABLE_CORE PBL_BIT(0)
#define USBHS_ENABLE_PHY  PBL_BIT(1)

/* VBUSVALID, and bit 31, undocumented, which holds the PHY at device */
#define USBHS_PHY_VBUSVALID PBL_BIT(30)
#define USBHS_PHY_DEVICE    PBL_BIT(31)

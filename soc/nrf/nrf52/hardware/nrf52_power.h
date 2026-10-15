/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_POWER_RESETREAS_OFFSET 0x0400

/* Register bit definitions */

#define POWER_RESETREAS_RESETPIN PBL_BIT(0)
#define POWER_RESETREAS_DOG      PBL_BIT(1)
#define POWER_RESETREAS_SREQ     PBL_BIT(2)
#define POWER_RESETREAS_LOCKUP   PBL_BIT(3)
#define POWER_RESETREAS_OFF      PBL_BIT(16)
#define POWER_RESETREAS_LPCOMP   PBL_BIT(17)
#define POWER_RESETREAS_DIF      PBL_BIT(18)
#define POWER_RESETREAS_NFC      PBL_BIT(19)
#define POWER_RESETREAS_VBUS     PBL_BIT(20)

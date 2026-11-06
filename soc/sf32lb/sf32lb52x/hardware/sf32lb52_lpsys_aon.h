/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_LPSYS_AON_PMR_OFFSET      0x0000
#define SF32LB52_LPSYS_AON_ACR_OFFSET      0x0010
#define SF32LB52_LPSYS_AON_SLP_CTRL_OFFSET 0x0040

/* Register bit definitions */

#define LPSYS_AON_PMR_CPUWAIT PBL_BIT(2)

#define LPSYS_AON_ACR_HXT48_REQ PBL_BIT(1)
#define LPSYS_AON_ACR_HXT48_RDY PBL_BIT(31)

#define LPSYS_AON_SLP_CTRL_WKUP_REQ     PBL_BIT(1)
#define LPSYS_AON_SLP_CTRL_SLEEP_STATUS PBL_BIT(4)

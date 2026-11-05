/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_HPSYS_CFG_SYSCR_OFFSET   0x0010
#define SF32LB52_HPSYS_CFG_RTC_TR_OFFSET  0x0014
#define SF32LB52_HPSYS_CFG_RTC_DR_OFFSET  0x0018
#define SF32LB52_HPSYS_CFG_ANAU_CR_OFFSET 0x0088

/* Register bit definitions */

#define HPSYS_CFG_SYSCR_WDT1_REBOOT PBL_BIT(0)

#define HPSYS_CFG_ANAU_CR_EN_BG PBL_BIT(0)

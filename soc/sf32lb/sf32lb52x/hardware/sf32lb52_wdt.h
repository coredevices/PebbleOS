/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_WDT_CVR0_OFFSET 0x0000
#define SF32LB52_WDT_CVR1_OFFSET 0x0004
#define SF32LB52_WDT_CR_OFFSET   0x0008
#define SF32LB52_WDT_CCR_OFFSET  0x000c
#define SF32LB52_WDT_ICR_OFFSET  0x0010
#define SF32LB52_WDT_SR_OFFSET   0x0014
#define SF32LB52_WDT_WP_OFFSET   0x0018
#define SF32LB52_WDT_FG_OFFSET   0x001c

/* Register bit definitions */

#define WDT_CR_RESET_LENGTH_MASK PBL_GENMASK(2, 0)
#define WDT_CR_RESPONSE_MODE     PBL_BIT(4)

#define WDT_CCR_START 0x76U
#define WDT_CCR_STOP  0x34U

#define WDT_ICR_INT_CLR PBL_BIT(0)

#define WDT_SR_INT_ASSERT PBL_BIT(0)
#define WDT_SR_WDT_ACTIVE PBL_BIT(1)

#define WDT_FG_RST_FG_CLR  PBL_BIT(0)
#define WDT_FG_RST_FG      PBL_BIT(1)
#define WDT_FG_SYNC_FG_CLR PBL_BIT(2)
#define WDT_FG_SYNC_FG     PBL_BIT(3)

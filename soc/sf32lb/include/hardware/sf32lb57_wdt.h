/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB57_WDT_CVR0_OFFSET 0x0000
#define SF32LB57_WDT_CR_OFFSET   0x0008
#define SF32LB57_WDT_CCR_OFFSET  0x000c
#define SF32LB57_WDT_SR_OFFSET   0x0014

/* Register bit definitions */

#define WDT_CR_RESPONSE_MODE PBL_BIT(4)

#define WDT_CCR_START 0x76U
#define WDT_CCR_STOP  0x34U

#define WDT_SR_WDT_ACTIVE PBL_BIT(2)

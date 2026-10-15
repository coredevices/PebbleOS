/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_HPSYS_CFG_SYSCR_OFFSET 0x0010

/* Register bit definitions */

#define HPSYS_CFG_SYSCR_WDT1_REBOOT PBL_BIT(0)

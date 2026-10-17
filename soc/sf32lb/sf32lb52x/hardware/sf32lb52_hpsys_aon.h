/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_HPSYS_AON_WCR_OFFSET 0x0028

/* Register bit definitions */

/* Wakeup pin n is pad PA24 + n */
#define HPSYS_AON_WCR_PIN(n) PBL_BIT(8 + (n))
#define HPSYS_AON_WCR_AON    PBL_BIT(31)

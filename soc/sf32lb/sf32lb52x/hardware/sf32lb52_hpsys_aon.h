/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_HPSYS_AON_CR1_OFFSET 0x0004
#define SF32LB52_HPSYS_AON_WCR_OFFSET 0x0028

/* Register bit definitions */

#define HPSYS_AON_CR1_PINOUT_SEL0_MASK      PBL_GENMASK(27, 25)
#define HPSYS_AON_CR1_PINOUT_SEL1_MASK      PBL_GENMASK(30, 28)
#define HPSYS_AON_CR1_PINOUT_SEL_LPTIM1_OUT 2U
#define HPSYS_AON_CR1_PINOUT_SEL_LPTIM2_OUT 3U

/* Wakeup pin n is pad PA24 + n */
#define HPSYS_AON_WCR_PIN(n) PBL_BIT(8 + (n))
#define HPSYS_AON_WCR_AON    PBL_BIT(31)

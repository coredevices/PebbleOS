/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_RTC_PBR_OFFSET(n) (0x005c + ((n) * 4))
#define SF32LB52_RTC_PAWK1R_OFFSET 0x006c
#define SF32LB52_RTC_PAWK2R_OFFSET 0x0070

/* Register bit definitions */

#define RTC_PBR_OE              PBL_BIT(1)
#define RTC_PBR_PE              PBL_BIT(3)
#define RTC_PBR_PS              PBL_BIT(4)
#define RTC_PBR_SEL_MASK        PBL_GENMASK(14, 12)
#define RTC_PBR_SEL_PBR_OUT     0U
#define RTC_PBR_SEL_CLK_RTC     1U
#define RTC_PBR_SEL_PINOUT_SEL0 2U
#define RTC_PBR_SEL_PINOUT_SEL1 3U

/* Pull enable (PAWK1R) and select (PAWK2R) of always-on pad PA28 + n */
#define RTC_PAWK_PIN(n) PBL_BIT(n)

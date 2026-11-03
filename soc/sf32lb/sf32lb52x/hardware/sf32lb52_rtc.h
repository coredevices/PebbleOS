/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_RTC_TR_OFFSET     0x0000
#define SF32LB52_RTC_DR_OFFSET     0x0004
#define SF32LB52_RTC_CR_OFFSET     0x0008
#define SF32LB52_RTC_ISR_OFFSET    0x000c
#define SF32LB52_RTC_PSCLR_OFFSET  0x0010
#define SF32LB52_RTC_PBR_OFFSET(n) (0x005c + ((n) * 4))
#define SF32LB52_RTC_PAWK1R_OFFSET 0x006c
#define SF32LB52_RTC_PAWK2R_OFFSET 0x0070

/* Register bit definitions */

#define RTC_TR_SS_MASK  PBL_GENMASK(9, 0)
#define RTC_TR_SU_MASK  PBL_GENMASK(14, 11)
#define RTC_TR_ST_MASK  PBL_GENMASK(17, 15)
#define RTC_TR_MNU_MASK PBL_GENMASK(21, 18)
#define RTC_TR_MNT_MASK PBL_GENMASK(24, 22)
#define RTC_TR_HU_MASK  PBL_GENMASK(28, 25)
#define RTC_TR_HT_MASK  PBL_GENMASK(30, 29)
#define RTC_TR_PM       PBL_BIT(31)

#define RTC_DR_DU_MASK PBL_GENMASK(3, 0)
#define RTC_DR_DT_MASK PBL_GENMASK(5, 4)
#define RTC_DR_MU_MASK PBL_GENMASK(11, 8)
#define RTC_DR_MT      PBL_BIT(12)
#define RTC_DR_WD_MASK PBL_GENMASK(15, 13)
#define RTC_DR_YU_MASK PBL_GENMASK(19, 16)
#define RTC_DR_YT_MASK PBL_GENMASK(23, 20)
#define RTC_DR_CB      PBL_BIT(24)
#define RTC_DR_ERR     PBL_BIT(31)

#define RTC_CR_FMT       PBL_BIT(6)
#define RTC_CR_POL       PBL_BIT(18)
#define RTC_CR_OSEL_MASK PBL_GENMASK(20, 19)

#define RTC_ISR_TSOVF PBL_BIT(5)
#define RTC_ISR_RSF   PBL_BIT(7)
#define RTC_ISR_INITF PBL_BIT(9)
#define RTC_ISR_INIT  PBL_BIT(10)

#define RTC_PSCLR_DIVB_MASK      PBL_GENMASK(9, 0)
#define RTC_PSCLR_DIVA_FRAC_MASK PBL_GENMASK(23, 10)
#define RTC_PSCLR_DIVA_INT_MASK  PBL_GENMASK(31, 24)

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

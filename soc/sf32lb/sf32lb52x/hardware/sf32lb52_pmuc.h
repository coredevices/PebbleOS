/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_PMUC_CR_OFFSET      0x0000
#define SF32LB52_PMUC_WER_OFFSET     0x0004
#define SF32LB52_PMUC_WSR_OFFSET     0x0008
#define SF32LB52_PMUC_WCR_OFFSET     0x000c
#define SF32LB52_PMUC_HXT_CR1_OFFSET 0x0068

/* Register bit definitions */

#define PMUC_WER_RTC    PBL_BIT(0)
#define PMUC_WER_WDT1   PBL_BIT(1)
#define PMUC_WER_WDT2   PBL_BIT(2)
#define PMUC_WER_PIN0   PBL_BIT(3)
#define PMUC_WER_PIN1   PBL_BIT(4)
#define PMUC_WER_LOWBAT PBL_BIT(7)
#define PMUC_WER_CHG    PBL_BIT(8)

#define PMUC_WSR_RTC    PBL_BIT(0)
#define PMUC_WSR_WDT1   PBL_BIT(1)
#define PMUC_WSR_WDT2   PBL_BIT(2)
#define PMUC_WSR_PIN0   PBL_BIT(3)
#define PMUC_WSR_PIN1   PBL_BIT(4)
#define PMUC_WSR_IWDT   PBL_BIT(5)
#define PMUC_WSR_PWRKEY PBL_BIT(6)
#define PMUC_WSR_LOWBAT PBL_BIT(7)
#define PMUC_WSR_CHG    PBL_BIT(8)

#define PMUC_HXT_CR1_BUF_AUD_EN PBL_BIT(8)

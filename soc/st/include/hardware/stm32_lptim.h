/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_LPTIM_ISR_OFFSET  0x0000
#define STM32_LPTIM_ICR_OFFSET  0x0004
#define STM32_LPTIM_IER_OFFSET  0x0008
#define STM32_LPTIM_CFGR_OFFSET 0x000c
#define STM32_LPTIM_CR_OFFSET   0x0010
#define STM32_LPTIM_CMP_OFFSET  0x0014
#define STM32_LPTIM_ARR_OFFSET  0x0018
#define STM32_LPTIM_CNT_OFFSET  0x001c

// ISR flags, cleared by the same bits in ICR, and enabled by them in IER
#define LPTIM_ISR_CMPM  PBL_BIT(0)
#define LPTIM_ISR_ARRM  PBL_BIT(1)
#define LPTIM_ISR_CMPOK PBL_BIT(3)
#define LPTIM_ISR_ARROK PBL_BIT(4)
#define LPTIM_ISR_ALL   PBL_GENMASK(6, 0)

#define LPTIM_CR_ENABLE  PBL_BIT(0)
#define LPTIM_CR_CNTSTRT PBL_BIT(2)

#define LPTIM_CNT_MASK PBL_GENMASK(15, 0)

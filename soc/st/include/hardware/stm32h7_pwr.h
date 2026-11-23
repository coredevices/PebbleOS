/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32H7_PWR_CR1_OFFSET   0x0000
#define STM32H7_PWR_CSR1_OFFSET  0x0004
#define STM32H7_PWR_CR3_OFFSET   0x000c
#define STM32H7_PWR_CPUCR_OFFSET 0x0010
#define STM32H7_PWR_D3CR_OFFSET  0x0018

/* Register bit definitions */

#define PWR_CR1_LPDS PBL_BIT(0)
#define PWR_CR1_DBP  PBL_BIT(8)
#define PWR_CR1_FLPS PBL_BIT(9)

#define PWR_CPUCR_PDDS_D1 PBL_BIT(0)
#define PWR_CPUCR_PDDS_D2 PBL_BIT(1)
#define PWR_CPUCR_PDDS_D3 PBL_BIT(2)
#define PWR_CPUCR_RUN_D3  PBL_BIT(11)

#define PWR_CSR1_ACTVOSRDY PBL_BIT(13)

#define PWR_D3CR_VOSRDY     PBL_BIT(13)
#define PWR_D3CR_VOS_MASK   PBL_GENMASK(15, 14)
#define PWR_D3CR_VOS_SCALE1 3U

#define PWR_CR3_BYPASS       PBL_BIT(0)
#define PWR_CR3_LDOEN        PBL_BIT(1)
#define PWR_CR3_SDEN         PBL_BIT(2)
#define PWR_CR3_SDEXTHP      PBL_BIT(3)
#define PWR_CR3_SDLEVEL_MASK PBL_GENMASK(5, 4)
#define PWR_CR3_SDLEVEL_1V8  1U
#define PWR_CR3_SDLEVEL_2V5  2U
#define PWR_CR3_SUPPLY_MASK  PBL_GENMASK(5, 0)
#define PWR_CR3_USB33DEN     PBL_BIT(24)
#define PWR_CR3_USB33RDY     PBL_BIT(26)

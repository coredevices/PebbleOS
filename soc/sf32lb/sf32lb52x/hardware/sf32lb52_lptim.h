/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_LPTIM_ISR_OFFSET  0x0000
#define SF32LB52_LPTIM_ICR_OFFSET  0x0004
#define SF32LB52_LPTIM_IER_OFFSET  0x0008
#define SF32LB52_LPTIM_CFGR_OFFSET 0x000c
#define SF32LB52_LPTIM_CR_OFFSET   0x0010
#define SF32LB52_LPTIM_CMP_OFFSET  0x0014
#define SF32LB52_LPTIM_ARR_OFFSET  0x0018
#define SF32LB52_LPTIM_CNT_OFFSET  0x001c

/* Register bit definitions */

#define LPTIM_CFGR_CKSEL      PBL_BIT(0)
#define LPTIM_CFGR_INTCKSEL   PBL_BIT(5)
#define LPTIM_CFGR_PRESC_MASK PBL_GENMASK(11, 9)
#define LPTIM_CFGR_WAVE       PBL_BIT(20)
#define LPTIM_CFGR_WAVPOL     PBL_BIT(21)

#define LPTIM_CR_ENABLE   PBL_BIT(0)
#define LPTIM_CR_CNTSTRT  PBL_BIT(2)
#define LPTIM_CR_COUNTRST PBL_BIT(3)

#define LPTIM_ARR_MASK PBL_GENMASK(23, 0)

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_LPSYS_RCC_RSTR1_OFFSET 0x0000
#define SF32LB52_LPSYS_RCC_CSR_OFFSET   0x0010

/* Register bit definitions */

#define LPSYS_RCC_RSTR1_LCPU PBL_BIT(0)
#define LPSYS_RCC_RSTR1_MAC  PBL_BIT(20)

#define LPSYS_RCC_CSR_SEL_SYS_MASK  PBL_GENMASK(1, 0)
#define LPSYS_RCC_CSR_SEL_SYS_HXT48 1U

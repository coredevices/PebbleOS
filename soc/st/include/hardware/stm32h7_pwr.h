/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32H7_PWR_CSR1_OFFSET 0x0004
#define STM32H7_PWR_CR3_OFFSET  0x000c

/* Register bit definitions */

#define PWR_CSR1_ACTVOSRDY PBL_BIT(13)

#define PWR_CR3_BYPASS       PBL_BIT(0)
#define PWR_CR3_LDOEN        PBL_BIT(1)
#define PWR_CR3_SDEN         PBL_BIT(2)
#define PWR_CR3_SDEXTHP      PBL_BIT(3)
#define PWR_CR3_SDLEVEL_MASK PBL_GENMASK(5, 4)
#define PWR_CR3_SDLEVEL_1V8  1U
#define PWR_CR3_SDLEVEL_2V5  2U
#define PWR_CR3_SUPPLY_MASK  PBL_GENMASK(5, 0)

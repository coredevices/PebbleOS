/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

#define SF32LB52_EFUSEC_NUM_BANKS 4U
#define SF32LB52_EFUSEC_BANK_SIZE 32U

/* Register offsets */

#define SF32LB52_EFUSEC_CR_OFFSET           0x0000
#define SF32LB52_EFUSEC_TIMR_OFFSET         0x0004
#define SF32LB52_EFUSEC_SR_OFFSET           0x0008
#define SF32LB52_EFUSEC_BANK_DATA_OFFSET(n) (0x0030 + ((n) << 5))

/* Register bit definitions */

#define EFUSEC_CR_EN           PBL_BIT(0)
#define EFUSEC_CR_BANKSEL_MASK PBL_GENMASK(3, 2)

#define EFUSEC_TIMR_THRCK_MASK PBL_GENMASK(6, 0)

#define EFUSEC_SR_DONE PBL_BIT(0)

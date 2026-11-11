/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB52_PATCH_CH_OFFSET(n) (0x0000 + (n) * 4)
#define SF32LB52_PATCH_CER_OFFSET   0x0080
#define SF32LB52_PATCH_CSR_OFFSET   0x0084
#define SF32LB52_PATCH_CDR_OFFSET   0x0088

/* Register bit definitions */

#define PATCH_CH_ADDR_MASK PBL_GENMASK(18, 2)

#define SF32LB52_PATCH_NUM_CHANNELS 32U

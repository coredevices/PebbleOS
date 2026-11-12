/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB57_HPSYS_CFG_ANAU_CR_OFFSET 0x0044

/* Register bit definitions */

#define HPSYS_CFG_ANAU_CR_EN_BG       PBL_BIT(0)
#define HPSYS_CFG_ANAU_CR_EN_VBAT_MON PBL_BIT(1)

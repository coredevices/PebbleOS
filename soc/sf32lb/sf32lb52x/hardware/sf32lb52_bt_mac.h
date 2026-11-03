/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_BT_MAC_RCCAL_CTRL_OFFSET   0x0114
#define SF32LB52_BT_MAC_RCCAL_RESULT_OFFSET 0x0118

/* Register bit definitions */

#define BT_MAC_RCCAL_CTRL_LENGTH_MASK  PBL_GENMASK(15, 0)
#define BT_MAC_RCCAL_CTRL_AUTO         PBL_BIT(16)
#define BT_MAC_RCCAL_CTRL_START        PBL_BIT(17)
#define BT_MAC_RCCAL_CTRL_STOP         PBL_BIT(18)
#define BT_MAC_RCCAL_CTRL_CON_NUM_MASK PBL_GENMASK(28, 19)
#define BT_MAC_RCCAL_CTRL_CON_MODE     PBL_BIT(29)

#define BT_MAC_RCCAL_RESULT_RESULT_MASK PBL_GENMASK(30, 0)
#define BT_MAC_RCCAL_RESULT_DONE        PBL_BIT(31)

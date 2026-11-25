/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_FICR_TRIMCNF_ADDR_OFFSET(n) (0x0400 + ((n) << 3))
#define NRF54L_FICR_TRIMCNF_DATA_OFFSET(n) (0x0404 + ((n) << 3))

/* Register bit definitions */

#define FICR_TRIMCNF_NUM 64U

#define FICR_XOSCTRIM_SLOPE_MASK  PBL_GENMASK(8, 0)
#define FICR_XOSCTRIM_OFFSET_MASK PBL_GENMASK(25, 16)

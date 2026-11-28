/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_VPR_CPURUN_OFFSET 0x0800
#define NRF54L_VPR_INITPC_OFFSET 0x0808

/* Register bit definitions */

#define VPR_CPURUN_EN PBL_BIT(0)

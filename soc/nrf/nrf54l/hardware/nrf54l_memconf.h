/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Register offsets */

#define NRF54L_MEMCONF_POWER_RET_OFFSET(n)  (0x0508 + 0x10 * (n))
#define NRF54L_MEMCONF_POWER_RET2_OFFSET(n) (0x050c + 0x10 * (n))

/* Register bit definitions */

#define NRF54L_MEMCONF_POWER_COUNT 2U

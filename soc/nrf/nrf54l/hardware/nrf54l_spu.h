/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_SPU_PERIPH_PERM_OFFSET(n) (0x0500 + ((n) << 2))

/* Register bit definitions */

#define SPU_PERIPH_PERM_SECATTR PBL_BIT(4)

// The peripheral of an address, its slave index on the bus of its SPU
#define SPU_SLAVE_INDEX(addr) (((addr) >> 12) & 0x3fU)

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_PINMUX_PAD_SA_OFFSET(n) (0x0000 + ((n) * 4))
#define SF32LB52_PINMUX_PAD_PA_OFFSET(n) (0x0034 + ((n) * 4))

/* Register bit definitions */

#define PINMUX_PAD_FSEL_MASK PBL_GENMASK(3, 0)
#define PINMUX_PAD_PE        PBL_BIT(4)
#define PINMUX_PAD_PS        PBL_BIT(5)
#define PINMUX_PAD_IE        PBL_BIT(6)
#define PINMUX_PAD_IS        PBL_BIT(7)
#define PINMUX_PAD_SR        PBL_BIT(8)
#define PINMUX_PAD_DS0       PBL_BIT(9)
#define PINMUX_PAD_DS1       PBL_BIT(10)
#define PINMUX_PAD_DS_MASK   PBL_GENMASK(10, 9)
#define PINMUX_PAD_POE       PBL_BIT(11)

#define PINMUX_PAD_FSEL_GPIO 0U

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_WWDG_CR_OFFSET 0x0000

#define WWDG_CR_T_MASK PBL_GENMASK(6, 0)
#define WWDG_CR_WDGA   PBL_BIT(7)

/* A counter without bit 6 resets: written so, at once */
#define WWDG_CR_T_RESET 0x3fU

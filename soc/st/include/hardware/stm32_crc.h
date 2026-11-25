/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_CRC_DR_OFFSET   0x0000
#define STM32_CRC_CR_OFFSET   0x0008
#define STM32_CRC_INIT_OFFSET 0x0010
#define STM32_CRC_POL_OFFSET  0x0014

#define CRC_CR_RESET         PBL_BIT(0)
#define CRC_CR_POLYSIZE_MASK PBL_GENMASK(4, 3)
#define CRC_CR_POLYSIZE_32   0U
#define CRC_CR_REV_IN_MASK   PBL_GENMASK(6, 5)
#define CRC_CR_REV_IN_WORD   3U
#define CRC_CR_REV_OUT       PBL_BIT(7)

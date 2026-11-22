/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_BDMA_NUM_CHANNELS 8U

#define STM32_BDMA_ISR_OFFSET      0x0000
#define STM32_BDMA_IFCR_OFFSET     0x0004
#define STM32_BDMA_CCR_OFFSET(c)   (0x0008 + 0x14 * (c))
#define STM32_BDMA_CNDTR_OFFSET(c) (0x000c + 0x14 * (c))
#define STM32_BDMA_CPAR_OFFSET(c)  (0x0010 + 0x14 * (c))
#define STM32_BDMA_CM0AR_OFFSET(c) (0x0014 + 0x14 * (c))

/* Flags of a channel in ISR and IFCR, at 4 bits per channel */
#define BDMA_ISR_GIF      PBL_BIT(0)
#define BDMA_ISR_TCIF     PBL_BIT(1)
#define BDMA_ISR_HTIF     PBL_BIT(2)
#define BDMA_ISR_TEIF     PBL_BIT(3)
#define BDMA_ISR_ALL      PBL_GENMASK(3, 0)
#define BDMA_ISR_SHIFT(c) (4U * (c))

#define BDMA_CCR_EN         PBL_BIT(0)
#define BDMA_CCR_TCIE       PBL_BIT(1)
#define BDMA_CCR_HTIE       PBL_BIT(2)
#define BDMA_CCR_TEIE       PBL_BIT(3)
#define BDMA_CCR_DIR        PBL_BIT(4)
#define BDMA_CCR_CIRC       PBL_BIT(5)
#define BDMA_CCR_MINC       PBL_BIT(7)
#define BDMA_CCR_PSIZE_MASK PBL_GENMASK(9, 8)
#define BDMA_CCR_MSIZE_MASK PBL_GENMASK(11, 10)
#define BDMA_CCR_PL_MASK    PBL_GENMASK(13, 12)

#define BDMA_CNDTR_MASK PBL_GENMASK(15, 0)

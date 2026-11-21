/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_DMA_NUM_STREAMS 8U

#define STM32_DMA_LISR_OFFSET  0x0000
#define STM32_DMA_HISR_OFFSET  0x0004
#define STM32_DMA_LIFCR_OFFSET 0x0008
#define STM32_DMA_HIFCR_OFFSET 0x000c

#define STM32_DMA_SCR_OFFSET(s)   (0x0010 + 0x18 * (s))
#define STM32_DMA_SNDTR_OFFSET(s) (0x0014 + 0x18 * (s))
#define STM32_DMA_SPAR_OFFSET(s)  (0x0018 + 0x18 * (s))
#define STM32_DMA_SM0AR_OFFSET(s) (0x001c + 0x18 * (s))

#define DMA_SCR_EN         PBL_BIT(0)
#define DMA_SCR_DMEIE      PBL_BIT(1)
#define DMA_SCR_TEIE       PBL_BIT(2)
#define DMA_SCR_HTIE       PBL_BIT(3)
#define DMA_SCR_TCIE       PBL_BIT(4)
#define DMA_SCR_DIR_MASK   PBL_GENMASK(7, 6)
#define DMA_SCR_DIR_P2M    0U
#define DMA_SCR_DIR_M2P    1U
#define DMA_SCR_CIRC       PBL_BIT(8)
#define DMA_SCR_MINC       PBL_BIT(10)
#define DMA_SCR_PSIZE_MASK PBL_GENMASK(12, 11)
#define DMA_SCR_MSIZE_MASK PBL_GENMASK(14, 13)
#define DMA_SCR_PL_MASK    PBL_GENMASK(17, 16)
#define DMA_SCR_TRBUFF     PBL_BIT(20)

#define DMA_SNDTR_MASK PBL_GENMASK(15, 0)

// Flags of a stream in LISR/HISR and LIFCR/HIFCR, at the offset of its place in its half
#define DMA_ISR_FEIF  PBL_BIT(0)
#define DMA_ISR_DMEIF PBL_BIT(2)
#define DMA_ISR_TEIF  PBL_BIT(3)
#define DMA_ISR_HTIF  PBL_BIT(4)
#define DMA_ISR_TCIF  PBL_BIT(5)
#define DMA_ISR_ALL   (DMA_ISR_FEIF | DMA_ISR_DMEIF | DMA_ISR_TEIF | DMA_ISR_HTIF | DMA_ISR_TCIF)

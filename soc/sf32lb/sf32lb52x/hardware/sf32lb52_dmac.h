/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

#define SF32LB52_DMAC_NUM_CHANNELS 8U

/* Register offsets, of channel n from 0 */

#define SF32LB52_DMAC_ISR_OFFSET      0x0000
#define SF32LB52_DMAC_IFCR_OFFSET     0x0004
#define SF32LB52_DMAC_CCR_OFFSET(n)   (0x0008 + (n) * 0x14)
#define SF32LB52_DMAC_CNDTR_OFFSET(n) (0x000c + (n) * 0x14)
#define SF32LB52_DMAC_CPAR_OFFSET(n)  (0x0010 + (n) * 0x14)
#define SF32LB52_DMAC_CM0AR_OFFSET(n) (0x0014 + (n) * 0x14)
#define SF32LB52_DMAC_CBSR_OFFSET(n)  (0x0018 + (n) * 0x14)
#define SF32LB52_DMAC_CSELR_OFFSET(n) (0x00a8 + ((n) / 4U) * 4U)

/* Register bit definitions */

#define DMAC_ISR_GIF(n)  PBL_BIT((n) * 4U)
#define DMAC_ISR_TCIF(n) PBL_BIT((n) * 4U + 1U)
#define DMAC_ISR_HTIF(n) PBL_BIT((n) * 4U + 2U)
#define DMAC_ISR_TEIF(n) PBL_BIT((n) * 4U + 3U)
#define DMAC_ISR_ALL(n)  (0xfU << ((n) * 4U))

#define DMAC_CCR_EN         PBL_BIT(0)
#define DMAC_CCR_TCIE       PBL_BIT(1)
#define DMAC_CCR_HTIE       PBL_BIT(2)
#define DMAC_CCR_TEIE       PBL_BIT(3)
#define DMAC_CCR_DIR        PBL_BIT(4)
#define DMAC_CCR_CIRC       PBL_BIT(5)
#define DMAC_CCR_PINC       PBL_BIT(6)
#define DMAC_CCR_MINC       PBL_BIT(7)
#define DMAC_CCR_PSIZE_MASK PBL_GENMASK(9, 8)
#define DMAC_CCR_MSIZE_MASK PBL_GENMASK(11, 10)
#define DMAC_CCR_PL_MASK    PBL_GENMASK(13, 12)
#define DMAC_CCR_MEM2MEM    PBL_BIT(14)

#define DMAC_CNDTR_NDT_MASK PBL_GENMASK(15, 0)

#define DMAC_CSELR_CS_POS(n)  (((n) % 4U) * 8U)
#define DMAC_CSELR_CS_MASK(n) (0x3fU << DMAC_CSELR_CS_POS(n))

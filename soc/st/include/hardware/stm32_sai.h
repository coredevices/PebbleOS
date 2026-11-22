/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets within the common registers */
#define STM32_SAI_PDMCR_OFFSET 0x0044

#define SAI_PDMCR_PDMEN       PBL_BIT(0)
#define SAI_PDMCR_MICNBR_MASK PBL_GENMASK(5, 4)
#define SAI_PDMCR_CKEN1       PBL_BIT(8)

/* Register offsets within a sub-block */
#define STM32_SAI_CR1_OFFSET   0x0000
#define STM32_SAI_CR2_OFFSET   0x0004
#define STM32_SAI_FRCR_OFFSET  0x0008
#define STM32_SAI_SLOTR_OFFSET 0x000c
#define STM32_SAI_SR_OFFSET    0x0014
#define STM32_SAI_CLRFR_OFFSET 0x0018
#define STM32_SAI_DR_OFFSET    0x001c

#define SAI_CR1_MODE_MASK      PBL_GENMASK(1, 0)
#define SAI_CR1_MODE_MASTER_TX 0U
#define SAI_CR1_MODE_MASTER_RX 1U
#define SAI_CR1_DS_MASK        PBL_GENMASK(7, 5)
#define SAI_CR1_DS_16          4U
#define SAI_CR1_LSBFIRST       PBL_BIT(8)
#define SAI_CR1_CKSTR          PBL_BIT(9)
#define SAI_CR1_MONO           PBL_BIT(12)
#define SAI_CR1_OUTDRIV        PBL_BIT(13)
#define SAI_CR1_SAIEN          PBL_BIT(16)
#define SAI_CR1_DMAEN          PBL_BIT(17)
#define SAI_CR1_NODIV          PBL_BIT(19)
#define SAI_CR1_MCKDIV_MASK    PBL_GENMASK(25, 20)
#define SAI_CR1_MCKEN          PBL_BIT(27)

#define SAI_CR2_FTH_MASK    PBL_GENMASK(2, 0)
#define SAI_CR2_FTH_QUARTER 1U
#define SAI_CR2_FFLUSH      PBL_BIT(3)

#define SAI_FRCR_FRL_MASK   PBL_GENMASK(7, 0)
#define SAI_FRCR_FSALL_MASK PBL_GENMASK(14, 8)
#define SAI_FRCR_FSDEF      PBL_BIT(16)
#define SAI_FRCR_FSPOL      PBL_BIT(17)
#define SAI_FRCR_FSOFF      PBL_BIT(18)

#define SAI_SLOTR_SLOTSZ_MASK PBL_GENMASK(7, 6)
#define SAI_SLOTR_SLOTSZ_32   2U
#define SAI_SLOTR_NBSLOT_MASK PBL_GENMASK(11, 8)
#define SAI_SLOTR_SLOTEN_MASK PBL_GENMASK(31, 16)

#define SAI_CLRFR_ALL PBL_GENMASK(6, 0)

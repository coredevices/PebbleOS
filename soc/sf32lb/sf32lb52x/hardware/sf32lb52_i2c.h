/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_I2C_CR_OFFSET   0x0000
#define SF32LB52_I2C_TCR_OFFSET  0x0004
#define SF32LB52_I2C_IER_OFFSET  0x0008
#define SF32LB52_I2C_SR_OFFSET   0x000c
#define SF32LB52_I2C_DBR_OFFSET  0x0010
#define SF32LB52_I2C_SAR_OFFSET  0x0014
#define SF32LB52_I2C_LCR_OFFSET  0x0018
#define SF32LB52_I2C_WCR_OFFSET  0x001c
#define SF32LB52_I2C_DNR_OFFSET  0x0028
#define SF32LB52_I2C_FIFO_OFFSET 0x0030

/* Register bit definitions */

#define I2C_CR_MODE_MASK PBL_GENMASK(1, 0)
#define I2C_CR_MODE_FS   1U
#define I2C_CR_IUE       PBL_BIT(2)
#define I2C_CR_SCLE      PBL_BIT(3)
#define I2C_CR_DMAEN     PBL_BIT(4)
#define I2C_CR_LASTNACK  PBL_BIT(5)
#define I2C_CR_LASTSTOP  PBL_BIT(6)
#define I2C_CR_MSDE      PBL_BIT(8)
#define I2C_CR_DNF_MASK  PBL_GENMASK(14, 12)
#define I2C_CR_RSTREQ    PBL_BIT(30)
#define I2C_CR_UR        PBL_BIT(31)

#define I2C_TCR_TB    PBL_BIT(0)
#define I2C_TCR_START PBL_BIT(1)
#define I2C_TCR_STOP  PBL_BIT(2)
#define I2C_TCR_NACK  PBL_BIT(3)
#define I2C_TCR_MA    PBL_BIT(4)
#define I2C_TCR_RXREQ PBL_BIT(6)

#define I2C_IER_TEIE      PBL_BIT(6)
#define I2C_IER_RFIE      PBL_BIT(7)
#define I2C_IER_BEDIE     PBL_BIT(10)
#define I2C_IER_MSDIE     PBL_BIT(12)
#define I2C_IER_DMADONEIE PBL_BIT(13)

#define I2C_SR_NACK    PBL_BIT(1)
#define I2C_SR_UB      PBL_BIT(2)
#define I2C_SR_TE      PBL_BIT(6)
#define I2C_SR_RF      PBL_BIT(7)
#define I2C_SR_BED     PBL_BIT(10)
#define I2C_SR_MSD     PBL_BIT(12)
#define I2C_SR_DMADONE PBL_BIT(13)

#define I2C_LCR_SLV_MASK PBL_GENMASK(8, 0)
#define I2C_LCR_FLV_MASK PBL_GENMASK(17, 9)

#define I2C_WCR_CNT_MASK PBL_GENMASK(7, 0)

#define I2C_DNR_NDT_MASK PBL_GENMASK(8, 0)

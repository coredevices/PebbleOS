/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB57_SPI_TOP_CTRL_OFFSET  0x0000
#define SF32LB57_SPI_FIFO_CTRL_OFFSET 0x0004
#define SF32LB57_SPI_INTE_OFFSET      0x0008
#define SF32LB57_SPI_STATUS_OFFSET    0x0010
#define SF32LB57_SPI_DATA_OFFSET      0x0014
#define SF32LB57_SPI_TOP_CTRL2_OFFSET 0x001c

/* Register bit definitions */

#define SPI_TOP_CTRL_SSE          PBL_BIT(0)
#define SPI_TOP_CTRL_DSS_MASK     PBL_GENMASK(9, 5)
#define SPI_TOP_CTRL_SPO          PBL_BIT(10)
#define SPI_TOP_CTRL_SPH          PBL_BIT(11)
#define SPI_TOP_CTRL_CLK_DIV_MASK PBL_GENMASK(25, 19)
#define SPI_TOP_CTRL_CLK_SSP_EN   PBL_BIT(27)

#define SPI_FIFO_CTRL_TFT_MASK PBL_GENMASK(4, 0)
#define SPI_FIFO_CTRL_RFT_MASK PBL_GENMASK(9, 5)
#define SPI_FIFO_CTRL_TSRE     PBL_BIT(10)

#define SPI_INTE_RIE PBL_BIT(2)
#define SPI_INTE_TIE PBL_BIT(3)
#define SPI_INTE_RIM PBL_BIT(4)
#define SPI_INTE_TIM PBL_BIT(5)

#define SPI_STATUS_BSY PBL_BIT(0)
#define SPI_STATUS_TNF PBL_BIT(6)
#define SPI_STATUS_TUR PBL_BIT(12)
#define SPI_STATUS_RNE PBL_BIT(14)
#define SPI_STATUS_ROR PBL_BIT(20)

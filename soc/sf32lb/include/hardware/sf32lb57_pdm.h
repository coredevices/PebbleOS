/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The SF32LB57 PDM: the SF32LB52 one with a compensation filter and two half-band filters in place
 * of the low-pass filter, which moves the registers after SINC_CFG and the channel enables of CFG0.
 * The other fields are those of hardware/sf32lb52_pdm.h.
 */

/* Register offsets */

#define SF32LB57_PDM_CFG0_OFFSET         0x0000
#define SF32LB57_PDM_SINC_CFG_OFFSET     0x0008
#define SF32LB57_PDM_COMP_FLT_CFG_OFFSET 0x000c
#define SF32LB57_PDM_HPF_CFG_OFFSET      0x0010
#define SF32LB57_PDM_PGA_CFG_OFFSET      0x0014
#define SF32LB57_PDM_HBF_CFG_OFFSET      0x0018
#define SF32LB57_PDM_FIFO_CFG_OFFSET     0x001c
#define SF32LB57_PDM_DATA_L_OFFSET       0x0020
#define SF32LB57_PDM_INT_ST_OFFSET       0x002c
#define SF32LB57_PDM_INT_MSK_OFFSET      0x0030
#define SF32LB57_PDM_INT_CLR_OFFSET      0x0034

/* Register bit definitions */

#define SF32LB57_PDM_CFG0_CLK_DIV_MASK PBL_GENMASK(8, 2)
#define SF32LB57_PDM_CFG0_LEFT_EN      PBL_BIT(9)
#define SF32LB57_PDM_CFG0_RIGHT_EN     PBL_BIT(10)
#define SF32LB57_PDM_CFG0_STEREO_EN    PBL_BIT(11)
#define SF32LB57_PDM_CFG0_SWAP_EN      PBL_BIT(12)

#define SF32LB57_PDM_COMP_FLT_CFG_COEFF_SEL   PBL_BIT(0)
#define SF32LB57_PDM_COMP_FLT_CFG_COMP_BYPASS PBL_BIT(1)

#define SF32LB57_PDM_HBF_CFG_HBF1_GAIN_EN PBL_BIT(0)
#define SF32LB57_PDM_HBF_CFG_HBF1_BYPASS  PBL_BIT(1)
#define SF32LB57_PDM_HBF_CFG_HBF2_GAIN_EN PBL_BIT(2)
#define SF32LB57_PDM_HBF_CFG_HBF2_BYPASS  PBL_BIT(3)

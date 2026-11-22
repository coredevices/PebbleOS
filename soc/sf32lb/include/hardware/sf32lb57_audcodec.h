/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The SF32LB57 AUDCODEC is the SF32LB52 one (sf32lb52_audcodec.h) with a single DAC channel and
 * output. A PLL dither register after the calibration result moves the analog ADC registers, and
 * the DAC output after them. The ADC reference moves to ADC_ANA_CFG, next to a power switch of the
 * ADCs, and PLL_CFG4 gains a second divider of the digital clock.
 */

/* Register offsets */

#define SF32LB57_AUDCODEC_ADC_ANA_CFG_OFFSET 0x00b0
#define SF32LB57_AUDCODEC_ADC1_CFG1_OFFSET   0x00b4
#define SF32LB57_AUDCODEC_ADC1_CFG2_OFFSET   0x00b8
#define SF32LB57_AUDCODEC_ADC2_CFG1_OFFSET   0x00bc
#define SF32LB57_AUDCODEC_ADC2_CFG2_OFFSET   0x00c0
#define SF32LB57_AUDCODEC_DAC1_CFG_OFFSET    0x00d4

/* Register bit definitions */

#define SF32LB57_AUDCODEC_PLL_CFG4_DIVB_CLK_DIG_MASK PBL_GENMASK(19, 18)
#define SF32LB57_AUDCODEC_PLL_CFG4_EN_CLK_DIG        PBL_BIT(25)

#define SF32LB57_AUDCODEC_ADC_ANA_CFG_PSW_EN PBL_BIT(0)

#define SF32LB57_AUDCODEC_ADCN_CFG1_VCMST PBL_BIT(4)

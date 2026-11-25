/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB52_GPTIM_CR1_OFFSET   0x0000
#define SF32LB52_GPTIM_CR2_OFFSET   0x0004
#define SF32LB52_GPTIM_SMCR_OFFSET  0x0008
#define SF32LB52_GPTIM_DIER_OFFSET  0x000c
#define SF32LB52_GPTIM_SR_OFFSET    0x0010
#define SF32LB52_GPTIM_EGR_OFFSET   0x0014
#define SF32LB52_GPTIM_CCMR1_OFFSET 0x0018
#define SF32LB52_GPTIM_CCMR2_OFFSET 0x001c
#define SF32LB52_GPTIM_CCER_OFFSET  0x0020
#define SF32LB52_GPTIM_CNT_OFFSET   0x0024
#define SF32LB52_GPTIM_PSC_OFFSET   0x0028
#define SF32LB52_GPTIM_ARR_OFFSET   0x002c
#define SF32LB52_GPTIM_CCR1_OFFSET  0x0034

/* Capture/compare register of channel 0-3 */
#define SF32LB52_GPTIM_CCR_OFFSET(ch) (SF32LB52_GPTIM_CCR1_OFFSET + 4U * (ch))
/* Capture/compare mode register of channel 0-3 */
#define SF32LB52_GPTIM_CCMR_OFFSET(ch) (SF32LB52_GPTIM_CCMR1_OFFSET + 4U * ((ch) / 2U))

/* Register bit definitions */

#define GPTIM_CR1_CEN  PBL_BIT(0)
#define GPTIM_CR1_ARPE PBL_BIT(7)

#define GPTIM_EGR_UG PBL_BIT(0)

/* Channel fields of CCMR1/CCMR2, by channel 0-3 */
#define GPTIM_CCMR_CCS_MASK(ch)   PBL_GENMASK(1U + 8U * ((ch) % 2U), 8U * ((ch) % 2U))
#define GPTIM_CCMR_OCPE(ch)       PBL_BIT(19U + 8U * ((ch) % 2U))
#define GPTIM_CCMR_OCM_MASK(ch)   PBL_GENMASK(23U + 8U * ((ch) % 2U), 20U + 8U * ((ch) % 2U))
#define GPTIM_CCMR_OCM_FORCE_LOW  4U
#define GPTIM_CCMR_OCM_FORCE_HIGH 5U
#define GPTIM_CCMR_OCM_PWM1       6U

/* Channel fields of CCER, by channel 0-3 */
#define GPTIM_CCER_CCE(ch) PBL_BIT(4U * (ch))
#define GPTIM_CCER_CCP(ch) PBL_BIT(4U * (ch) + 1U)

#define GPTIM_PSC_MASK PBL_GENMASK(15, 0)
#define GPTIM_ARR_MASK PBL_GENMASK(15, 0)

#define GPTIM_NUM_CHANNELS 4U

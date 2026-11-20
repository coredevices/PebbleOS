/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets of a general-purpose timer */

#define STM32_TIM_CR1_OFFSET   0x0000
#define STM32_TIM_DIER_OFFSET  0x000c
#define STM32_TIM_SR_OFFSET    0x0010
#define STM32_TIM_EGR_OFFSET   0x0014
#define STM32_TIM_CCMR1_OFFSET 0x0018
#define STM32_TIM_CCER_OFFSET  0x0020
#define STM32_TIM_CNT_OFFSET   0x0024
#define STM32_TIM_PSC_OFFSET   0x0028
#define STM32_TIM_ARR_OFFSET   0x002c
#define STM32_TIM_CCR1_OFFSET  0x0034

/* Register bit definitions */

#define TIM_CR1_CEN PBL_BIT(0)
#define TIM_CR1_URS PBL_BIT(2)

#define TIM_DIER_CC1IE PBL_BIT(1)

#define TIM_SR_UIF   PBL_BIT(0)
#define TIM_SR_CC1IF PBL_BIT(1)

#define TIM_EGR_UG   PBL_BIT(0)
#define TIM_EGR_CC1G PBL_BIT(1)

#define TIM_PSC_MAX 0xffffU

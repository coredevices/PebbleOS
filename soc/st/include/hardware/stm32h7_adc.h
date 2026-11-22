/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32H7_ADC_ISR_OFFSET   0x0000
#define STM32H7_ADC_CR_OFFSET    0x0008
#define STM32H7_ADC_CFGR_OFFSET  0x000c
#define STM32H7_ADC_SMPR1_OFFSET 0x0014
#define STM32H7_ADC_SMPR2_OFFSET 0x0018
#define STM32H7_ADC_PCSEL_OFFSET 0x001c
#define STM32H7_ADC_SQR1_OFFSET  0x0030
#define STM32H7_ADC_DR_OFFSET    0x0040

// Common registers, from the base of the ADC block
#define STM32H7_ADC_CCR_OFFSET 0x0308

#define ADC_ISR_ADRDY PBL_BIT(0)
#define ADC_ISR_EOC   PBL_BIT(2)
#define ADC_ISR_EOS   PBL_BIT(3)
#define ADC_ISR_OVR   PBL_BIT(4)

#define ADC_CR_ADEN       PBL_BIT(0)
#define ADC_CR_ADDIS      PBL_BIT(1)
#define ADC_CR_ADSTART    PBL_BIT(2)
#define ADC_CR_BOOST_MASK PBL_GENMASK(9, 8)
#define ADC_CR_ADCALLIN   PBL_BIT(16)
#define ADC_CR_ADVREGEN   PBL_BIT(28)
#define ADC_CR_DEEPPWD    PBL_BIT(29)
#define ADC_CR_ADCALDIF   PBL_BIT(30)
#define ADC_CR_ADCAL      PBL_BIT(31)

#define ADC_CFGR_OVRMOD PBL_BIT(12)
#define ADC_CFGR_JQDIS  PBL_BIT(31)

#define ADC_SMPR_SMP_BITS 3U
#define ADC_SMPR_SMP_MAX  7U

#define ADC_SQR1_SQ1_MASK PBL_GENMASK(10, 6)

#define ADC_CCR_CKMODE_MASK  PBL_GENMASK(17, 16)
#define ADC_CCR_CKMODE_HCLK4 3U
#define ADC_CCR_VREFEN       PBL_BIT(22)
#define ADC_CCR_TSEN         PBL_BIT(23)

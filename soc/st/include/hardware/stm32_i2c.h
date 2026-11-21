/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_I2C_CR1_OFFSET     0x0000
#define STM32_I2C_CR2_OFFSET     0x0004
#define STM32_I2C_TIMINGR_OFFSET 0x0010
#define STM32_I2C_ISR_OFFSET     0x0018
#define STM32_I2C_ICR_OFFSET     0x001c
#define STM32_I2C_RXDR_OFFSET    0x0024
#define STM32_I2C_TXDR_OFFSET    0x0028

#define I2C_CR1_PE     PBL_BIT(0)
#define I2C_CR1_TXIE   PBL_BIT(1)
#define I2C_CR1_RXIE   PBL_BIT(2)
#define I2C_CR1_NACKIE PBL_BIT(4)
#define I2C_CR1_STOPIE PBL_BIT(5)
#define I2C_CR1_TCIE   PBL_BIT(6)
#define I2C_CR1_ERRIE  PBL_BIT(7)

#define I2C_CR2_SADD_MASK   PBL_GENMASK(9, 0)
#define I2C_CR2_RD_WRN      PBL_BIT(10)
#define I2C_CR2_START       PBL_BIT(13)
#define I2C_CR2_STOP        PBL_BIT(14)
#define I2C_CR2_NBYTES_MASK PBL_GENMASK(23, 16)
#define I2C_CR2_RELOAD      PBL_BIT(24)

#define I2C_TIMINGR_SCLL_MASK   PBL_GENMASK(7, 0)
#define I2C_TIMINGR_SCLH_MASK   PBL_GENMASK(15, 8)
#define I2C_TIMINGR_SDADEL_MASK PBL_GENMASK(19, 16)
#define I2C_TIMINGR_SCLDEL_MASK PBL_GENMASK(23, 20)
#define I2C_TIMINGR_PRESC_MASK  PBL_GENMASK(31, 28)

// ISR flags; those that can be cleared are cleared by the same bits in ICR, and writing TXE
// empties the transmit register
#define I2C_ISR_TXE   PBL_BIT(0)
#define I2C_ISR_TXIS  PBL_BIT(1)
#define I2C_ISR_RXNE  PBL_BIT(2)
#define I2C_ISR_NACKF PBL_BIT(4)
#define I2C_ISR_STOPF PBL_BIT(5)
#define I2C_ISR_TC    PBL_BIT(6)
#define I2C_ISR_TCR   PBL_BIT(7)
#define I2C_ISR_BERR  PBL_BIT(8)
#define I2C_ISR_ARLO  PBL_BIT(9)
#define I2C_ISR_OVR   PBL_BIT(10)
#define I2C_ISR_BUSY  PBL_BIT(15)
#define I2C_ICR_ALL   PBL_GENMASK(13, 3)

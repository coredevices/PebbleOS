/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/reset.h>

/**
 * @defgroup drivers_reset_sf32lb SF32LB RCC resets
 * @ingroup drivers_reset
 * @brief @ref drivers_reset controller of the SF32LB52 HPSYS RCC.
 *
 * Reset numbers are (register index << 5) | bit, register 0 being RSTR1.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_reset_controller pbl_reset_sf32lb_hpsys_rcc;
/** @endcond */

/** HPSYS RCC reset controller. */
#define SF32LB_HPSYS_RCC_RESET (&pbl_reset_sf32lb_hpsys_rcc)

/** @name HPSYS RCC reset lines
 * @{
 */
#define SF32LB52_HPSYS_RST_DMAC1    0
#define SF32LB52_HPSYS_RST_MAILBOX1 1
#define SF32LB52_HPSYS_RST_PINMUX1  2
#define SF32LB52_HPSYS_RST_USART1   3
#define SF32LB52_HPSYS_RST_USART2   4
#define SF32LB52_HPSYS_RST_EZIP1    5
#define SF32LB52_HPSYS_RST_EPIC     6
#define SF32LB52_HPSYS_RST_LCDC1    7
#define SF32LB52_HPSYS_RST_I2S1     8
#define SF32LB52_HPSYS_RST_SYSCFG1  10
#define SF32LB52_HPSYS_RST_EFUSEC   11
#define SF32LB52_HPSYS_RST_AES      12
#define SF32LB52_HPSYS_RST_CRC1     13
#define SF32LB52_HPSYS_RST_TRNG     14
#define SF32LB52_HPSYS_RST_GPTIM1   15
#define SF32LB52_HPSYS_RST_GPTIM2   16
#define SF32LB52_HPSYS_RST_BTIM1    17
#define SF32LB52_HPSYS_RST_BTIM2    18
#define SF32LB52_HPSYS_RST_SPI1     20
#define SF32LB52_HPSYS_RST_SPI2     21
#define SF32LB52_HPSYS_RST_EXTDMA   22
#define SF32LB52_HPSYS_RST_PDM1     25
#define SF32LB52_HPSYS_RST_I2C1     27
#define SF32LB52_HPSYS_RST_I2C2     28
#define SF32LB52_HPSYS_RST_PTC1     31
#define SF32LB52_HPSYS_RST_GPIO1    32
#define SF32LB52_HPSYS_RST_MPI1     33
#define SF32LB52_HPSYS_RST_MPI2     34
#define SF32LB52_HPSYS_RST_SDMMC1   36
#define SF32LB52_HPSYS_RST_USBC     38
#define SF32LB52_HPSYS_RST_I2C3     40
#define SF32LB52_HPSYS_RST_ATIM1    41
#define SF32LB52_HPSYS_RST_USART3   44
#define SF32LB52_HPSYS_RST_AUDCODEC 51
#define SF32LB52_HPSYS_RST_AUDPRC   52
#define SF32LB52_HPSYS_RST_GPADC    54
#define SF32LB52_HPSYS_RST_TSEN     55
#define SF32LB52_HPSYS_RST_I2C4     57
/** @} */

/** @} */

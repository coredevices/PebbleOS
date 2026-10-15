/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_sf32lb SF32LB RCC clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller of the SF32LB52 HPSYS RCC.
 *
 * Clock numbers are (register index << 5) | bit, register 0 being ENR1.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_sf32lb_hpsys_rcc;
/** @endcond */

/** HPSYS RCC clock controller. */
#define SF32LB_HPSYS_RCC (&pbl_clk_sf32lb_hpsys_rcc)

/** @name HPSYS RCC clocks
 * @{
 */
#define SF32LB52_HPSYS_CLK_DMAC1    0
#define SF32LB52_HPSYS_CLK_MAILBOX1 1
#define SF32LB52_HPSYS_CLK_PINMUX1  2
#define SF32LB52_HPSYS_CLK_USART2   4
#define SF32LB52_HPSYS_CLK_EZIP1    5
#define SF32LB52_HPSYS_CLK_EPIC     6
#define SF32LB52_HPSYS_CLK_LCDC1    7
#define SF32LB52_HPSYS_CLK_I2S1     8
#define SF32LB52_HPSYS_CLK_SYSCFG1  10
#define SF32LB52_HPSYS_CLK_EFUSEC   11
#define SF32LB52_HPSYS_CLK_AES      12
#define SF32LB52_HPSYS_CLK_CRC1     13
#define SF32LB52_HPSYS_CLK_TRNG     14
#define SF32LB52_HPSYS_CLK_GPTIM1   15
#define SF32LB52_HPSYS_CLK_GPTIM2   16
#define SF32LB52_HPSYS_CLK_BTIM1    17
#define SF32LB52_HPSYS_CLK_BTIM2    18
#define SF32LB52_HPSYS_CLK_SPI1     20
#define SF32LB52_HPSYS_CLK_SPI2     21
#define SF32LB52_HPSYS_CLK_EXTDMA   22
#define SF32LB52_HPSYS_CLK_SECU1    23
#define SF32LB52_HPSYS_CLK_PDM1     25
#define SF32LB52_HPSYS_CLK_I2C1     27
#define SF32LB52_HPSYS_CLK_I2C2     28
#define SF32LB52_HPSYS_CLK_PTC1     31
#define SF32LB52_HPSYS_CLK_GPIO1    32
#define SF32LB52_HPSYS_CLK_MPI1     33
#define SF32LB52_HPSYS_CLK_MPI2     34
#define SF32LB52_HPSYS_CLK_SDMMC1   36
#define SF32LB52_HPSYS_CLK_USBC     38
#define SF32LB52_HPSYS_CLK_I2C3     40
#define SF32LB52_HPSYS_CLK_ATIM1    41
#define SF32LB52_HPSYS_CLK_USART3   44
#define SF32LB52_HPSYS_CLK_AUDCODEC 51
#define SF32LB52_HPSYS_CLK_AUDPRC   52
#define SF32LB52_HPSYS_CLK_GPADC    54
#define SF32LB52_HPSYS_CLK_TSEN     55
#define SF32LB52_HPSYS_CLK_I2C4     57
/** @} */

/** @} */

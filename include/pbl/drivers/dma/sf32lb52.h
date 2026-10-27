/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/dma.h>

/**
 * @defgroup drivers_dma_sf32lb52 SF32LB52 DMAC
 * @ingroup drivers_dma
 * @brief @ref drivers_dma controller of the SF32LB52 HPSYS (DMAC1).
 *
 * Channels are numbered from 0, so channel 5 is the one the reference manual and the interrupt
 * line call CH6. The configuration of a channel is its priority, @c SF32LB52_DMA_PL_*. Each
 * channel has its own interrupt line, connected by the board only for channels whose consumers
 * set a callback:
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_rx =
 *     PBL_DMA_CHAN(SF32LB52_DMAC1, 5, SF32LB52_DMA_REQ_I2C2, SF32LB52_DMA_PL_HIGH);
 * PBL_IRQ_CONNECT(DMAC1_CH6, 5, pbl_dma_sf32lb52_irq_handler, &s_rx, 0);
 * @endcode
 *
 * The controller clock stays on once the controller is initialized.
 * @{
 */

/**
 * @name Channel configuration
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define SF32LB52_DMA_PL_MSK 0x3U
/** @endcond */
/** Low priority. */
#define SF32LB52_DMA_PL_LOW 0U
/** Medium priority. */
#define SF32LB52_DMA_PL_MEDIUM 1U
/** High priority. */
#define SF32LB52_DMA_PL_HIGH 2U
/** Very high priority. */
#define SF32LB52_DMA_PL_VERY_HIGH 3U
/** @} */

/**
 * @name Request lines
 * @{
 */
#define SF32LB52_DMA_REQ_MPI1              0U
#define SF32LB52_DMA_REQ_MPI2              1U
#define SF32LB52_DMA_REQ_I2C4              3U
#define SF32LB52_DMA_REQ_USART1_TX         4U
#define SF32LB52_DMA_REQ_USART1_RX         5U
#define SF32LB52_DMA_REQ_USART2_TX         6U
#define SF32LB52_DMA_REQ_USART2_RX         7U
#define SF32LB52_DMA_REQ_GPTIM1_UPDATE     8U
#define SF32LB52_DMA_REQ_GPTIM1_TRIGGER    9U
#define SF32LB52_DMA_REQ_GPTIM1_CC1        10U
#define SF32LB52_DMA_REQ_GPTIM1_CC2        11U
#define SF32LB52_DMA_REQ_GPTIM1_CC3        12U
#define SF32LB52_DMA_REQ_GPTIM1_CC4        13U
#define SF32LB52_DMA_REQ_BTIM1             14U
#define SF32LB52_DMA_REQ_BTIM2             15U
#define SF32LB52_DMA_REQ_ATIM1_UPDATE      16U
#define SF32LB52_DMA_REQ_ATIM1_TRIGGER     17U
#define SF32LB52_DMA_REQ_ATIM1_CC1         18U
#define SF32LB52_DMA_REQ_ATIM1_CC2         19U
#define SF32LB52_DMA_REQ_ATIM1_CC3         20U
#define SF32LB52_DMA_REQ_ATIM1_CC4         21U
#define SF32LB52_DMA_REQ_I2C1              22U
#define SF32LB52_DMA_REQ_I2C2              23U
#define SF32LB52_DMA_REQ_I2C3              24U
#define SF32LB52_DMA_REQ_ATIM1_COM         25U
#define SF32LB52_DMA_REQ_USART3_TX         26U
#define SF32LB52_DMA_REQ_USART3_RX         27U
#define SF32LB52_DMA_REQ_SPI1_TX           28U
#define SF32LB52_DMA_REQ_SPI1_RX           29U
#define SF32LB52_DMA_REQ_SPI2_TX           30U
#define SF32LB52_DMA_REQ_SPI2_RX           31U
#define SF32LB52_DMA_REQ_I2S1_TX           32U
#define SF32LB52_DMA_REQ_I2S1_RX           33U
#define SF32LB52_DMA_REQ_PDM1_L            36U
#define SF32LB52_DMA_REQ_PDM1_R            37U
#define SF32LB52_DMA_REQ_GPADC             38U
#define SF32LB52_DMA_REQ_AUDADC_CH0        39U
#define SF32LB52_DMA_REQ_AUDADC_CH1        40U
#define SF32LB52_DMA_REQ_AUDAC_CH0         41U
#define SF32LB52_DMA_REQ_AUDAC_CH1         42U
#define SF32LB52_DMA_REQ_GPTIM2_UPDATE     43U
#define SF32LB52_DMA_REQ_GPTIM2_TRIGGER    44U
#define SF32LB52_DMA_REQ_GPTIM2_CC1        45U
#define SF32LB52_DMA_REQ_AUDPRC_TX_OUT_CH1 46U
#define SF32LB52_DMA_REQ_AUDPRC_TX_OUT_CH0 47U
#define SF32LB52_DMA_REQ_AUDPRC_TX_CH3     48U
#define SF32LB52_DMA_REQ_AUDPRC_TX_CH2     49U
#define SF32LB52_DMA_REQ_AUDPRC_TX_CH1     50U
#define SF32LB52_DMA_REQ_AUDPRC_TX_CH0     51U
#define SF32LB52_DMA_REQ_AUDPRC_RX_CH1     52U
#define SF32LB52_DMA_REQ_AUDPRC_RX_CH0     53U
#define SF32LB52_DMA_REQ_GPTIM2_CC2        54U
#define SF32LB52_DMA_REQ_GPTIM2_CC3        55U
#define SF32LB52_DMA_REQ_GPTIM2_CC4        56U
#define SF32LB52_DMA_REQ_SDMMC1            57U
/** @} */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_dma_controller pbl_dma_sf32lb52_dmac1;
/** @endcond */

/** HPSYS DMA controller. */
#define SF32LB52_DMAC1 (&pbl_dma_sf32lb52_dmac1)

/**
 * @brief Channel interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param chan Channel on @ref SF32LB52_DMAC1.
 */
void pbl_dma_sf32lb52_irq_handler(const struct pbl_dma_chan *chan);

/** @} */

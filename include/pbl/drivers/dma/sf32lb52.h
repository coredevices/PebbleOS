/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_dma_sf32lb52 SF32LB52 DMAC
 * @ingroup drivers_dma
 * @brief @ref drivers_dma controller of the SF32LB52 HPSYS (DMAC1).
 *
 * Channels are numbered from 0, so channel 5 is the one the reference manual and the interrupt
 * line call CH6. The configuration of a channel is its priority, @c SF32LB52_DMA_PL_*. Each
 * channel has its own interrupt line: the controller connects them all, with priority
 * @c CONFIG_DMA_SF32LB52_IRQ_PRIO, and enables a channel's while a transfer with a callback runs.
 *
 * @code{.c}
 * PBL_DMA_SF32LB52_DEFINE(s_dmac1, "dmac1", SF32LB52_DMAC1_BASE,
 *                         PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_DMAC1), 50, 51, 52, 53,
 *                         54, 55, 56, 57);
 * static const struct pbl_dma_chan s_rx =
 *     PBL_DMA_CHAN(&s_dmac1, 5, SF32LB52_DMA_REQ_I2C2, SF32LB52_DMA_PL_HIGH);
 * @endcode
 *
 * Cyclic transfers run a ring of two periods, ended by the half and full transfer interrupts.
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

/** @brief Number of channels. */
#define PBL_DMA_SF32LB52_NUM_CHANNELS 8U

/** @brief SF32LB52 DMA controller. */
struct pbl_dma_sf32lb52 {
  /** DMA controller. */
  struct pbl_dma_controller ctrl;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** Interrupt line of each channel. */
  pbl_irq_t irqs[PBL_DMA_SF32LB52_NUM_CHANNELS];
};

/** @brief A channel of a controller, as its interrupt handler gets it. */
struct pbl_dma_sf32lb52_chan_irq {
  /** Controller. */
  const struct pbl_dma_sf32lb52 *dmac;
  /** Channel number, from 0. */
  uint8_t channel;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_dma_controller_ops pbl_dma_sf32lb52_ops;
int pbl_dma_sf32lb52_init(const struct pbl_device *dev);
void pbl_dma_sf32lb52_irq_handler(const struct pbl_dma_sf32lb52_chan_irq *irq);

#define PBL_DMA_SF32LB52_CHAN_IRQ(sym, _ch, _line)                                       \
  static const struct pbl_dma_sf32lb52_chan_irq sym##_irq##_ch = {&sym##_inst, _ch};     \
  PBL_IRQ_CONNECT_NUM(_line, CONFIG_DMA_SF32LB52_IRQ_PRIO, pbl_dma_sf32lb52_irq_handler, \
                      &sym##_irq##_ch, 0)
/** @endcond */

/**
 * @brief Define an SF32LB52 DMA controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _irq0 Interrupt line of channel 0, followed by those of channels 1 to 7.
 */
#define PBL_DMA_SF32LB52_DEFINE(sym, _name, _base, _clk, _irq0, _irq1, _irq2, _irq3, _irq4, _irq5, \
                                _irq6, _irq7)                                                      \
  PBL_DMA_CONTROLLER_STATE_DEFINE(sym, PBL_DMA_SF32LB52_NUM_CHANNELS);                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_dma_sf32lb52, struct pbl_dma_controller, ctrl){              \
    .ctrl = PBL_DMA_CONTROLLER_INIT(sym, _name, pbl_dma_sf32lb52_init, &pbl_dma_sf32lb52_ops),     \
    .base = (_base),                                                                               \
    .clk = _clk,                                                                                   \
    .irqs = {_irq0, _irq1, _irq2, _irq3, _irq4, _irq5, _irq6, _irq7},                              \
  };                                                                                               \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 0, _irq0);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 1, _irq1);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 2, _irq2);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 3, _irq3);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 4, _irq4);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 5, _irq5);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 6, _irq6);                                                        \
  PBL_DMA_SF32LB52_CHAN_IRQ(sym, 7, _irq7)

/** @} */

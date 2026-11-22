/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_dma_stm32h7_bdma STM32H7 BDMA
 * @ingroup drivers_dma
 * @brief @ref drivers_dma controller made of the DMAMUX2 request router and the BDMA behind it,
 * in the D3 domain.
 *
 * Channels are the request lines of DMAMUX2, from the reference manual, as consumers name them
 * in the devicetree. The first slave configuration of a request binds it to a free channel of
 * the BDMA, which it keeps; there are 8. The configuration of a channel is the priority, bits 16
 * and 17 as for DMA1 and DMA2.
 *
 * Each channel has its own interrupt line: the controller connects them all, with priority
 * @c CONFIG_DMA_STM32H7_BDMA_IRQ_PRIO, and enables a channel's while a transfer with a callback
 * runs. Cyclic transfers run a ring of two periods, ended by the half and full transfer
 * interrupts. Buffers must be in the memories of the D3 domain, as the SRAM4.
 * @{
 */

/** @brief Request lines of DMAMUX2, 1 to 31; 0 is none. */
#define PBL_DMA_STM32H7_BDMA_NUM_REQUESTS 32U

/** @brief Channels of the BDMA. */
#define PBL_DMA_STM32H7_BDMA_NUM_CHANNELS 8U

/** @brief Channel state. */
struct pbl_dma_stm32h7_bdma_chan {
  /** Request bound to the channel, 0 when free. */
  uint8_t request;
  /** Priority, a value of the PL field. */
  uint8_t priority;
};

/** @brief Controller. */
struct pbl_dma_stm32h7_bdma {
  /** DMA controller. */
  struct pbl_dma_controller ctrl;
  /** DMAMUX2 registers. */
  uintptr_t mux;
  /** BDMA registers. */
  uintptr_t bdma;
  /** BDMA clock. */
  struct pbl_clk clk;
  /** Interrupt line of each channel. */
  pbl_irq_t irqs[PBL_DMA_STM32H7_BDMA_NUM_CHANNELS];
  /** State of each channel. */
  struct pbl_dma_stm32h7_bdma_chan *chans;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_dma_controller_ops pbl_dma_stm32h7_bdma_ops;
int pbl_dma_stm32h7_bdma_init(const struct pbl_device *dev);

/** @brief A channel of the controller, as its interrupt handler gets it. */
struct pbl_dma_stm32h7_bdma_chan_irq {
  /** Controller. */
  const struct pbl_dma_stm32h7_bdma *dmac;
  /** Channel, from 0. */
  uint8_t chan;
};

void pbl_dma_stm32h7_bdma_irq_handler(const struct pbl_dma_stm32h7_bdma_chan_irq *irq);

#define PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, _c, _line)                                            \
  static const struct pbl_dma_stm32h7_bdma_chan_irq sym##_irq##_c = {&sym##_inst, _c};           \
  PBL_IRQ_CONNECT_NUM(_line, CONFIG_DMA_STM32H7_BDMA_IRQ_PRIO, pbl_dma_stm32h7_bdma_irq_handler, \
                      &sym##_irq##_c, 0)
/** @endcond */

/**
 * @brief Define the controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _mux DMAMUX2 registers.
 * @param _bdma BDMA registers.
 * @param _clk BDMA clock, from PBL_CLK().
 * @param _i0 Interrupt line of channel 0, followed by those of channels 1 to 7.
 */
#define PBL_DMA_STM32H7_BDMA_DEFINE(sym, _name, _mux, _bdma, _clk, _i0, _i1, _i2, _i3, _i4, _i5, \
                                    _i6, _i7)                                                    \
  PBL_DMA_CONTROLLER_STATE_DEFINE(sym, PBL_DMA_STM32H7_BDMA_NUM_REQUESTS);                       \
  static struct pbl_dma_stm32h7_bdma_chan sym##_chans[PBL_DMA_STM32H7_BDMA_NUM_CHANNELS];        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_dma_stm32h7_bdma, struct pbl_dma_controller, ctrl){        \
    .ctrl = PBL_DMA_CONTROLLER_INIT(sym, _name, pbl_dma_stm32h7_bdma_init, nullptr,              \
                                    &pbl_dma_stm32h7_bdma_ops),                                  \
    .mux = (_mux),                                                                               \
    .bdma = (_bdma),                                                                             \
    .clk = _clk,                                                                                 \
    .irqs = {_i0, _i1, _i2, _i3, _i4, _i5, _i6, _i7},                                            \
    .chans = sym##_chans,                                                                        \
  };                                                                                             \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 0, _i0);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 1, _i1);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 2, _i2);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 3, _i3);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 4, _i4);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 5, _i5);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 6, _i6);                                                    \
  PBL_DMA_STM32H7_BDMA_CHAN_IRQ(sym, 7, _i7)

/** @} */

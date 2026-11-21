/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_dma_stm32h7 STM32H7 DMA
 * @ingroup drivers_dma
 * @brief @ref drivers_dma controller made of the DMAMUX1 request router and the DMA1 and DMA2
 * controllers behind it.
 *
 * Channels are the request lines of DMAMUX1, from the reference manual, as consumers name them
 * in the devicetree. The first slave configuration of a request binds it to a free stream of
 * DMA1 or DMA2, which it keeps; there are 16. The configuration of a channel is a mask of
 * stream configuration bits: the priority, @c DMA_SCR_PL_MASK, and the bufferable transfers the
 * USARTs need, @c DMA_SCR_TRBUFF.
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_rx = PBL_DMA_CHAN(&s_dmamux1, 41, 41, DMA_SCR_TRBUFF);
 * @endcode
 *
 * Each stream has its own interrupt line: the controller connects them all, with priority
 * @c CONFIG_DMA_STM32H7_IRQ_PRIO, and enables a stream's while a transfer with a callback runs.
 * Cyclic transfers run a ring of two periods, ended by the half and full transfer interrupts.
 * Buffers must be in memory the DMA controllers reach: not in the DTCM.
 * @{
 */

/** @brief Request lines of DMAMUX1, 1 to 107; 0 is none. */
#define PBL_DMA_STM32H7_NUM_REQUESTS 108U

/** @brief Streams, 8 of DMA1 then 8 of DMA2. */
#define PBL_DMA_STM32H7_NUM_STREAMS 16U

/** @brief Stream state. */
struct pbl_dma_stm32h7_stream {
  /** Request bound to the stream, 0 when free. */
  uint8_t request;
  /** Configuration of its channel. */
  uint32_t config;
};

/** @brief Controller. */
struct pbl_dma_stm32h7 {
  /** DMA controller. */
  struct pbl_dma_controller ctrl;
  /** DMAMUX1 registers. */
  uintptr_t mux;
  /** DMA1 and DMA2 registers. */
  uintptr_t dma[2];
  /** DMA1 and DMA2 clocks. */
  struct pbl_clk clk[2];
  /** Interrupt line of each stream. */
  pbl_irq_t irqs[PBL_DMA_STM32H7_NUM_STREAMS];
  /** State of each stream. */
  struct pbl_dma_stm32h7_stream *streams;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_dma_controller_ops pbl_dma_stm32h7_ops;
int pbl_dma_stm32h7_init(const struct pbl_device *dev);

/** @brief A stream of the controller, as its interrupt handler gets it. */
struct pbl_dma_stm32h7_stream_irq {
  /** Controller. */
  const struct pbl_dma_stm32h7 *dmac;
  /** Stream, from 0. */
  uint8_t stream;
};

void pbl_dma_stm32h7_irq_handler(const struct pbl_dma_stm32h7_stream_irq *irq);

#define PBL_DMA_STM32H7_STREAM_IRQ(sym, _s, _line)                                     \
  static const struct pbl_dma_stm32h7_stream_irq sym##_irq##_s = {&sym##_inst, _s};    \
  PBL_IRQ_CONNECT_NUM(_line, CONFIG_DMA_STM32H7_IRQ_PRIO, pbl_dma_stm32h7_irq_handler, \
                      &sym##_irq##_s, 0)
/** @endcond */

/**
 * @brief Define the controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _mux DMAMUX1 registers.
 * @param _dma1 DMA1 registers.
 * @param _dma2 DMA2 registers.
 * @param _clk1 DMA1 clock, from PBL_CLK().
 * @param _clk2 DMA2 clock, from PBL_CLK().
 * @param _i0 Interrupt line of DMA1 stream 0, followed by those of its streams 1 to 7, then
 * those of DMA2 streams 0 to 7.
 */
#define PBL_DMA_STM32H7_DEFINE(sym, _name, _mux, _dma1, _dma2, _clk1, _clk2, _i0, _i1, _i2, _i3,  \
                               _i4, _i5, _i6, _i7, _i8, _i9, _i10, _i11, _i12, _i13, _i14, _i15)  \
  PBL_DMA_CONTROLLER_STATE_DEFINE(sym, PBL_DMA_STM32H7_NUM_REQUESTS);                             \
  static struct pbl_dma_stm32h7_stream sym##_streams[PBL_DMA_STM32H7_NUM_STREAMS];                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_dma_stm32h7, struct pbl_dma_controller, ctrl){              \
    .ctrl =                                                                                       \
        PBL_DMA_CONTROLLER_INIT(sym, _name, pbl_dma_stm32h7_init, nullptr, &pbl_dma_stm32h7_ops), \
    .mux = (_mux),                                                                                \
    .dma = {(_dma1), (_dma2)},                                                                    \
    .clk = {_clk1, _clk2},                                                                        \
    .irqs =                                                                                       \
        {_i0, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10, _i11, _i12, _i13, _i14, _i15},   \
    .streams = sym##_streams,                                                                     \
  };                                                                                              \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 0, _i0);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 1, _i1);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 2, _i2);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 3, _i3);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 4, _i4);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 5, _i5);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 6, _i6);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 7, _i7);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 8, _i8);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 9, _i9);                                                        \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 10, _i10);                                                      \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 11, _i11);                                                      \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 12, _i12);                                                      \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 13, _i13);                                                      \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 14, _i14);                                                      \
  PBL_DMA_STM32H7_STREAM_IRQ(sym, 15, _i15)

/** @} */

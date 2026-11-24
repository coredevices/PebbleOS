/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/spi.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_spi_sf32lb57 SiFli SF32LB57 SPI
 * @ingroup drivers_spi
 * @brief @ref drivers_spi driver for the SiFli SF32LB57 SPI controller.
 *
 * The controller shifts 8-bit frames, most significant bit first, so it does not support
 * @ref PBL_SPI_LSB_FIRST. Chip selects are GPIOs. Writes of @ref SPI_SF32LB57_DMA_MIN_BYTES or
 * more go through the transmit DMA channel when there is one; other transfers are moved through
 * the FIFOs from the controller's interrupt.
 *
 * @code{.c}
 * PBL_SPI_SF32LB57_DEFINE(s_spi1, "spi1", 0x50095000, 60,
 *                         PBL_CLK(&s_hpsys_rcc, SF32LB57_HPSYS_RCC_CLK_SPI1), s_spi1_pins,
 *                         &s_spi1_tx_dma, PBL_GPIO(&s_gpio1, 3, 0));
 * @endcode
 * @{
 */

/** @brief Shortest write that goes through DMA. */
#define SPI_SF32LB57_DMA_MIN_BYTES 32U

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_sf32lb57_state {
  const struct pbl_spi_xfer *xfer;
  uint32_t tx_pos;
  uint32_t rx_pos;
  uint32_t top_ctrl;
};
/** @endcond */

/** @brief SiFli SF32LB57 SPI bus. */
struct pbl_spi_sf32lb57 {
  /** SPI bus. */
  struct pbl_spi_bus bus;
  /** Driver runtime state. */
  struct pbl_spi_sf32lb57_state *state;
  /** SCLK, DIO and DI pins. */
  struct pbl_pinctrl pinctrl;
  /** Controller clock. */
  struct pbl_clk clk;
  /** Transmit DMA channel, or NULL. */
  const struct pbl_dma_chan *dma_tx;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_bus_ops pbl_spi_sf32lb57_ops;
/** @endcond */

/**
 * @brief Define a SiFli SF32LB57 SPI bus.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _base Register base address.
 * @param _irq Interrupt line.
 * @param _clk Controller clock, from PBL_CLK().
 * @param _pinctrl Pin states.
 * @param _dma_tx Transmit DMA channel, or NULL.
 * @param ... Chip select GPIOs, from PBL_GPIO().
 */
#define PBL_SPI_SF32LB57_DEFINE(sym, _name, _base, _irq, _clk, _pinctrl, _dma_tx, ...)  \
  PBL_SPI_BUS_STATE_DEFINE(sym, __VA_ARGS__);                                           \
  static struct pbl_spi_sf32lb57_state sym##_sf32lb57_state;                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_sf32lb57, struct pbl_spi_bus, bus){           \
    .bus = PBL_SPI_BUS_INIT(sym, _name, nullptr, &pbl_spi_sf32lb57_ops),                \
    .state = &sym##_sf32lb57_state,                                                     \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                       \
    .clk = _clk,                                                                        \
    .dma_tx = (_dma_tx),                                                                \
    .base = (_base),                                                                    \
    .irq = (_irq),                                                                      \
  };                                                                                    \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SPI_SF32LB57_IRQ_PRIO, pbl_spi_sf32lb57_irq_handler, \
                      &sym##_inst, 0)

/** @cond INTERNAL_HIDDEN */
void pbl_spi_sf32lb57_irq_handler(const struct pbl_spi_sf32lb57 *spi);
/** @endcond */

/** @} */

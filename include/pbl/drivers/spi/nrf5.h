/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/spi.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_spi_nrf5 nRF5 SPI
 * @ingroup drivers_spi
 * @brief @ref drivers_spi bus driver for the nRF5 SPIM peripheral.
 *
 * Transfers run through EasyDMA, so their buffers must be in data RAM, and are at most 65535
 * bytes. The clock runs at 125 kHz to 8 MHz, or up to 32 MHz on SPIM3.
 *
 * SCK and MOSI are outputs and MISO an input with its wiring flags, e.g. a pull, and keep that
 * configuration while the bus is disabled; MOSI and MISO may be left unconnected. An instance
 * connects its interrupt itself, with priority @c CONFIG_SPI_NRF5_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_SPI_NRF5_DEFINE(s_spi3, "spi3", NRF52_SPIM3_BASE, PBL_SOC_IRQN_SPIM3,
 *                     PBL_GPIO(&dt_gpio0, 6, 0), PBL_GPIO(&dt_gpio0, 8, 0), PBL_GPIO(NULL, 0, 0),
 *                     PBL_GPIO(&dt_gpio1, 3, 0));
 * @endcode
 * @{
 */

/** @brief nRF5 SPI bus. */
struct pbl_spi_nrf5 {
  /** SPI bus. */
  struct pbl_spi_bus bus;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** SCK pin. */
  struct pbl_gpio sck;
  /** MOSI pin, or unconnected. */
  struct pbl_gpio mosi;
  /** MISO pin, or unconnected. */
  struct pbl_gpio miso;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_bus_ops pbl_spi_nrf5_ops;
/** @endcond */

/**
 * @brief Define an nRF5 SPI bus.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _sck SCK pin, from PBL_GPIO().
 * @param _mosi MOSI pin, from PBL_GPIO(), or unconnected.
 * @param _miso MISO pin, from PBL_GPIO(), or unconnected.
 * @param ... Chip select pins, from PBL_GPIO(), by chip select number.
 */
#define PBL_SPI_NRF5_DEFINE(sym, _name, _base, _irq, _sck, _mosi, _miso, ...) \
  PBL_SPI_BUS_STATE_DEFINE(sym, __VA_ARGS__);                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_nrf5, struct pbl_spi_bus, bus){     \
    .bus = PBL_SPI_BUS_INIT(sym, _name, &pbl_spi_nrf5_ops, nullptr),          \
    .base = (_base),                                                          \
    .irq = (_irq),                                                            \
    .sck = _sck,                                                              \
    .mosi = _mosi,                                                            \
    .miso = _miso,                                                            \
  };                                                                          \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SPI_NRF5_IRQ_PRIO, pbl_spi_nrf5_irq_handler, &sym##_inst, 0)

/**
 * @brief SPIM interrupt handler, connected by PBL_SPI_NRF5_DEFINE().
 *
 * @param spi Bus.
 */
void pbl_spi_nrf5_irq_handler(const struct pbl_spi_nrf5 *spi);

/** @} */

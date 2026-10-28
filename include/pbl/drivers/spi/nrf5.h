/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/spi.h>

/**
 * @defgroup drivers_spi_nrf5 nRF5 SPI
 * @ingroup drivers_spi
 * @brief @ref drivers_spi bus driver for the nRF5 SPIM peripheral.
 *
 * Transfers run through EasyDMA, so their buffers must be in data RAM, and are at most 65535
 * bytes. The clock runs at 125 kHz to 8 MHz, or up to 32 MHz on SPIM3.
 *
 * SCK and MOSI are outputs and MISO an input with its wiring flags, e.g. a pull, and keep that
 * configuration while the bus is disabled; MOSI and MISO may be left unconnected. The SPIM
 * interrupt is connected by the board:
 *
 * @code{.c}
 * PBL_SPI_NRF5_DEFINE(s_spi3, "spi3", 3, PBL_GPIO(NRF5_GPIO_P0, 6, 0),
 *                     PBL_GPIO(NRF5_GPIO_P0, 8, 0), PBL_GPIO(NULL, 0, 0), NULL,
 *                     PBL_GPIO(NRF5_GPIO_P1, 3, PBL_GPIO_ACTIVE_LOW));
 * PBL_IRQ_CONNECT(SPIM3, 7, pbl_spi_nrf5_irq_handler, &s_spi3, 0);
 * @endcode
 * @{
 */

/** @brief nRF5 SPI bus. */
struct pbl_spi_nrf5 {
  /** SPI bus. */
  struct pbl_spi_bus bus;
  /** SPIM instance index. */
  uint8_t index;
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
 * @brief Define a bus on an nRF5 SPIM instance.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _idx SPIM instance index.
 * @param _sck SCK pin, from PBL_GPIO().
 * @param _mosi MOSI pin, from PBL_GPIO().
 * @param _miso MISO pin, from PBL_GPIO().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param ... Chip select GPIOs, from PBL_GPIO().
 */
#define PBL_SPI_NRF5_DEFINE(sym, _name, _idx, _sck, _mosi, _miso, _deps, ...) \
  PBL_SPI_BUS_STATE_DEFINE(sym, __VA_ARGS__);                                 \
  const struct pbl_spi_nrf5 sym = {                                           \
    .bus = PBL_SPI_BUS_INIT(sym, _name, &pbl_spi_nrf5_ops, _deps),            \
    .index = (_idx),                                                          \
    .sck = _sck,                                                              \
    .mosi = _mosi,                                                            \
    .miso = _miso,                                                            \
  };                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.bus.dev)

/**
 * @brief SPIM interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param spi Bus.
 */
void pbl_spi_nrf5_irq_handler(const struct pbl_spi_nrf5 *spi);

/** @} */

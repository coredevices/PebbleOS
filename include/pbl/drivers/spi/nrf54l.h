/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/spi.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_spi_nrf54l Nordic nRF54L SPIM
 * @ingroup drivers_spi
 * @brief SPI master with EasyDMA of the nRF54L.
 * @{
 */

/** @brief SPIM instance. */
struct pbl_spi_nrf54l {
  /** SPI bus. */
  struct pbl_spi_bus bus;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Core clock, which the prescaler divides, in Hz. */
  uint32_t clock_hz;
  /** Highest SCK frequency, in Hz. */
  uint32_t max_hz;
  /** SCK pin. */
  struct pbl_gpio sck;
  /** MOSI pin, or unconnected. */
  struct pbl_gpio mosi;
  /** MISO pin, or unconnected. */
  struct pbl_gpio miso;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_bus_ops pbl_spi_nrf54l_ops;

void pbl_spi_nrf54l_irq_handler(const struct pbl_spi_nrf54l *spi);
/** @endcond */

/**
 * @brief Define a SPIM instance.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _clock_hz Core clock, in Hz.
 * @param _max_hz Highest SCK frequency, in Hz.
 * @param _sck SCK pin, from PBL_GPIO().
 * @param _mosi MOSI pin, from PBL_GPIO().
 * @param _miso MISO pin, from PBL_GPIO().
 * @param ... Chip select pins, from PBL_GPIO().
 */
#define PBL_SPI_NRF54L_DEFINE(sym, _name, _base, _irq, _clock_hz, _max_hz, _sck, _mosi, _miso, \
                              ...)                                                             \
  PBL_SPI_BUS_STATE_DEFINE(sym, __VA_ARGS__);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_nrf54l, struct pbl_spi_bus, bus){                    \
    .bus = PBL_SPI_BUS_INIT(sym, _name, nullptr, &pbl_spi_nrf54l_ops),                         \
    .base = (_base),                                                                           \
    .irq = (_irq),                                                                             \
    .clock_hz = (_clock_hz),                                                                   \
    .max_hz = (_max_hz),                                                                       \
    .sck = _sck,                                                                               \
    .mosi = _mosi,                                                                             \
    .miso = _miso,                                                                             \
  };                                                                                           \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SPI_NRF54L_IRQ_PRIO, pbl_spi_nrf54l_irq_handler, &sym##_inst, 0)

/** @} */

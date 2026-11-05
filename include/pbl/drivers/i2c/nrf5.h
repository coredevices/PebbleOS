/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>

/**
 * @defgroup drivers_i2c_nrf5 nRF5 I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c bus driver for the nRF5 TWIM peripheral.
 *
 * The TWIM issues one write, one read, or a write then a read after a repeated START; writes
 * continued with @ref PBL_I2C_MSG_NOSTART are gathered in a buffer of
 * @ref I2C_NRF5_BOUNCE_BUF_SIZE bytes. Other sequences fail with @c -ENOTSUP.
 *
 * SCL and SDA are configured as open-drain inputs with their wiring flags, e.g. the pull-up, and
 * keep that configuration while the bus is disabled. The TWIM interrupt is connected by the
 * board:
 *
 * @code{.c}
 * PBL_I2C_NRF5_DEFINE(s_i2c0, "i2c0", 0, 400000, PBL_GPIO(NRF5_GPIO_P0, 25, PBL_GPIO_PULL_UP),
 *                     PBL_GPIO(NRF5_GPIO_P0, 11, PBL_GPIO_PULL_UP), NULL);
 * PBL_IRQ_CONNECT(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0, 7, pbl_i2c_nrf5_irq_handler, &s_i2c0, 0);
 * @endcode
 * @{
 */

/** @brief Longest write made of several messages, which the driver gathers in one buffer. */
#define I2C_NRF5_BOUNCE_BUF_SIZE 16

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_nrf5_state {
  uint32_t frequency;
  int error;
  bool tx;
  bool rx;
  uint32_t tx_len;
  uint32_t rx_len;
  uint8_t bounce[I2C_NRF5_BOUNCE_BUF_SIZE];
};
/** @endcond */

/** @brief nRF5 I2C bus. */
struct pbl_i2c_nrf5 {
  /** I2C bus. */
  struct pbl_i2c_bus bus;
  /** Driver runtime state. */
  struct pbl_i2c_nrf5_state *state;
  /** TWIM instance index. */
  uint8_t index;
  /** Bus clock frequency, in Hz. */
  uint32_t clock_hz;
  /** SCL pin. */
  struct pbl_gpio scl;
  /** SDA pin. */
  struct pbl_gpio sda;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_nrf5_ops;
/** @endcond */

/**
 * @brief Define a bus on an nRF5 TWIM instance.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _idx TWIM instance index.
 * @param _clock_hz Bus clock frequency: 100000, 250000 or 400000.
 * @param _scl SCL pin, from PBL_GPIO().
 * @param _sda SDA pin, from PBL_GPIO().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_NRF5_DEFINE(sym, _name, _idx, _clock_hz, _scl, _sda, _deps) \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                            \
  static struct pbl_i2c_nrf5_state sym##_nrf5_state;                        \
  const struct pbl_i2c_nrf5 sym = {                                         \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_nrf5_ops, _deps),          \
    .state = &sym##_nrf5_state,                                             \
    .index = (_idx),                                                        \
    .clock_hz = (_clock_hz),                                                \
    .scl = _scl,                                                            \
    .sda = _sda,                                                            \
  };                                                                        \
  PBL_DEVICE_REGISTER(sym, &sym.bus.dev)

/**
 * @brief TWIM interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param i2c Bus.
 */
void pbl_i2c_nrf5_irq_handler(const struct pbl_i2c_nrf5 *i2c);

/** @} */

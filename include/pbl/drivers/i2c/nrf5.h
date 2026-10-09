/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>

#include <nrfx_twim.h>

/**
 * @defgroup drivers_i2c_nrf5 nRF5 I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c bus driver for the nRF5 TWIM peripheral.
 *
 * The TWIM interrupt is connected by the board, to the nrfx handler:
 *
 * @code{.c}
 * PBL_I2C_NRF5_DEFINE(s_i2c0, "i2c0", 0, NRF_TWIM_FREQ_400K, PBL_GPIO(NRF5_GPIO_P0, 25, 0),
 *                     PBL_GPIO(NRF5_GPIO_P0, 11, 0), NULL);
 * PBL_IRQ_CONNECT(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0, NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY,
 *                 nrfx_twim_0_irq_handler, , 0);
 * @endcode
 * @{
 */

/** @brief Longest register write, register address included. */
#define I2C_NRF5_REG_WRITE_BUF_SIZE 16

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_nrf5_state {
  uint8_t reg_write_buf[I2C_NRF5_REG_WRITE_BUF_SIZE];
};
/** @endcond */

/** @brief nRF5 I2C bus. */
struct pbl_i2c_nrf5 {
  /** I2C bus. */
  struct pbl_i2c_bus bus;
  /** Driver runtime state. */
  struct pbl_i2c_nrf5_state *state;
  /** TWIM instance. */
  nrfx_twim_t twim;
  /** Bus clock frequency. */
  nrf_twim_frequency_t frequency;
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
 * @param _freq Bus clock frequency, an @c nrf_twim_frequency_t.
 * @param _scl SCL pin, from PBL_GPIO().
 * @param _sda SDA pin, from PBL_GPIO().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_NRF5_DEFINE(sym, _name, _idx, _freq, _scl, _sda, _deps) \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                        \
  static struct pbl_i2c_nrf5_state sym##_nrf5_state;                    \
  const struct pbl_i2c_nrf5 sym = {                                     \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_nrf5_ops, _deps),      \
    .state = &sym##_nrf5_state,                                         \
    .twim = NRFX_TWIM_INSTANCE(_idx),                                   \
    .frequency = (_freq),                                               \
    .scl = _scl,                                                        \
    .sda = _sda,                                                        \
  };                                                                    \
  PBL_DEVICE_REGISTER(sym, &sym.bus.dev)

/** @} */

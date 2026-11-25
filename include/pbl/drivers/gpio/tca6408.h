/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_gpio_tca6408 TCA6408
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio port over the eight pins of a TI TCA6408 I2C expander.
 *
 * Pins are push-pull, without pulls.
 * @{
 */

/** Number of pins. */
#define TCA6408_NUM_PINS 8U

/** @cond INTERNAL_HIDDEN */
struct pbl_tca6408_state {
  struct pbl_mutex lock;
  uint8_t output;
  uint8_t config;
};
/** @endcond */

/** @brief TCA6408 port. */
struct pbl_tca6408 {
  /** GPIO port, child of the I2C bus. */
  struct pbl_gpio_port port;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Runtime state. */
  struct pbl_tca6408_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_port_ops pbl_tca6408_ops;
int pbl_tca6408_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a TCA6408.
 *
 * Pins are then referenced with @c PBL_GPIO(&sym.port, pin, flags).
 *
 * @param sym Symbol of the port.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_TCA6408_DEFINE(sym, _bus, _addr)                                                     \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  static struct pbl_tca6408_state sym##_tca6408_state;                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_tca6408, struct pbl_gpio_port, port) {                     \
    .port = PBL_GPIO_PORT_INIT(sym, "tca6408", pbl_tca6408_init, &(_bus)->dev, &pbl_tca6408_ops, \
                               TCA6408_NUM_PINS, nullptr),                                       \
    .i2c = PBL_I2C_DEV(_bus, _addr), .state = &sym##_tca6408_state,                              \
  }

/** @} */

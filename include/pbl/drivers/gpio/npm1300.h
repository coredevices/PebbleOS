/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/mfd/npm1300.h>

/**
 * @defgroup drivers_gpio_npm1300 nPM1300 GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio port over the five GPIOs of the nPM1300, a cell of
 * @ref drivers_mfd_npm1300.
 *
 * A pin's pull-up is only enabled while it drives high.
 * @{
 */

/** Number of GPIOs. */
#define NPM1300_GPIO_NUM_PINS 5U

/** @cond INTERNAL_HIDDEN */
struct pbl_npm1300_gpio_state {
  uint8_t pullup_mask;
};
/** @endcond */

/** @brief nPM1300 GPIO port. */
struct pbl_npm1300_gpio {
  /** GPIO port, child of the core. */
  struct pbl_gpio_port port;
  /** Runtime state. */
  struct pbl_npm1300_gpio_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_port_ops pbl_npm1300_gpio_ops;
/** @endcond */

/**
 * @brief Define the GPIO port.
 *
 * Pins are then referenced with @c PBL_GPIO(&sym.port, pin, flags).
 *
 * @param sym Symbol of the port.
 * @param _pmic Core, a struct pbl_npm1300.
 */
#define PBL_NPM1300_GPIO_DEFINE(sym, _pmic)                                                        \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  static struct pbl_npm1300_gpio_state sym##_npm1300_gpio_state;                                   \
  const struct pbl_npm1300_gpio sym = {                                                            \
    .port = PBL_GPIO_PORT_INIT(sym, "npm1300_gpio", nullptr, &(_pmic)->dev, &pbl_npm1300_gpio_ops, \
                               NPM1300_GPIO_NUM_PINS, nullptr),                                    \
    .state = &sym##_npm1300_gpio_state,                                                            \
  };                                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.port.dev)

/** @} */

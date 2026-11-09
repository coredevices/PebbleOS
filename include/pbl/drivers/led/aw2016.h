/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/led.h>

/**
 * @defgroup drivers_led_aw2016 AW2016
 * @ingroup drivers_led
 * @brief Awinic AW2016 RGB LED controller, driving one multicolor @ref drivers_led on its three
 * channels. The chip is shut down while the LED is off.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_aw2016_state {
  bool enabled;
};
/** @endcond */

/** @brief AW2016 RGB LED controller. */
struct pbl_aw2016 {
  /** LED, child of the I2C bus. */
  struct pbl_led led;
  /** Runtime state. */
  struct pbl_aw2016_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_led_ops pbl_aw2016_ops;
int pbl_aw2016_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an AW2016 and its LED.
 *
 * @param sym Symbol of the LED.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_AW2016_DEFINE(sym, _bus, _addr)                                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                                         \
  static struct pbl_aw2016_state sym##_aw2016_state;                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_aw2016, struct pbl_led, led) {                    \
    .led = PBL_LED_INIT(sym, "aw2016", pbl_aw2016_init, &(_bus)->dev, &pbl_aw2016_ops), \
    .state = &sym##_aw2016_state, .i2c = PBL_I2C_DEV(_bus, _addr),                      \
  }

/** @} */

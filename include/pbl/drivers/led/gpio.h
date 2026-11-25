/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/led.h>

/**
 * @defgroup drivers_led_gpio GPIO
 * @ingroup drivers_led
 * @brief @ref drivers_led driver for an LED on a GPIO.
 *
 * The LED is on at any brightness but 0, whatever the color, and off when it comes up.
 * @{
 */

/** @brief LED on a GPIO. */
struct pbl_led_gpio {
  /** LED. */
  struct pbl_led led;
  /** GPIO, active when the LED is on. */
  struct pbl_gpio gpio;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_led_ops pbl_led_gpio_ops;
int pbl_led_gpio_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an LED on a GPIO.
 *
 * @param sym Symbol of the LED.
 * @param _name Name.
 * @param _gpio GPIO, from PBL_GPIO().
 */
#define PBL_LED_GPIO_DEFINE(sym, _name, _gpio)                                                     \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_led_gpio, struct pbl_led, led) {                             \
    .led = PBL_LED_INIT(sym, _name, pbl_led_gpio_init, nullptr, &pbl_led_gpio_ops), .gpio = _gpio, \
  }

/** @} */

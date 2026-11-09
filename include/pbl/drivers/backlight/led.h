/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/led.h>

/**
 * @defgroup drivers_backlight_led LED backlight
 * @ingroup drivers_backlight
 * @brief @ref drivers_backlight on an @ref drivers_led, its color mixed by multicolor LEDs.
 * @{
 */

/** @brief Backlight on an LED. */
struct pbl_backlight_led {
  /** Backlight. */
  struct pbl_backlight backlight;
  /** LED. */
  const struct pbl_led *led;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_backlight_led_ops;
/** @endcond */

/**
 * @brief Define a backlight on an LED.
 *
 * @param sym Symbol of the backlight.
 * @param _name Name.
 * @param _led LED.
 */
#define PBL_BACKLIGHT_LED_DEFINE(sym, _name, _led)                                      \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_backlight_led, struct pbl_backlight, backlight) { \
    .backlight = PBL_BACKLIGHT_INIT(sym, _name, &(_led)->dev, &pbl_backlight_led_ops),  \
    .led = (_led),                                                                      \
  }

/** @} */

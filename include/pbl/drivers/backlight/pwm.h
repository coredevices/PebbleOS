/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_backlight_pwm PWM backlight
 * @ingroup drivers_backlight
 * @brief @ref drivers_backlight driver driving the LED with a PWM output.
 *
 * The duty cycle follows the brightness linearly, full brightness being 100%: a lower ceiling
 * for the product is set above, e.g. by @c CONFIG_SERVICE_LIGHT_BACKLIGHT_ON_PERCENT.
 *
 * @code{.c}
 * PBL_BACKLIGHT_PWM_DEFINE(s_backlight, "backlight", PBL_GPIO(&dt_gpio1, 8, 0),
 *                          PBL_PWM(&s_pwm0, 0, PBL_PWM_HZ(256), 0));
 *
 * const struct pbl_backlight *const BACKLIGHT = &s_backlight;
 * @endcode
 * @{
 */

/** @brief PWM backlight. */
struct pbl_backlight_pwm {
  /** Backlight. */
  struct pbl_backlight backlight;
  /** Optional enable output, active while the backlight is on. */
  struct pbl_gpio enable;
  /** PWM output, with the period to run it at. */
  struct pbl_pwm pwm;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_backlight_pwm_ops;
/** @endcond */

/**
 * @brief Define a PWM backlight.
 *
 * @param sym Symbol of the backlight.
 * @param _name Name.
 * @param _enable Enable output, from PBL_GPIO(), or unconnected.
 * @param ... PWM output, from PBL_PWM(), with the period to run it at.
 */
#define PBL_BACKLIGHT_PWM_DEFINE(sym, _name, _enable, ...)                              \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_backlight_pwm, struct pbl_backlight, backlight) { \
    .backlight = PBL_BACKLIGHT_INIT(sym, _name, nullptr, &pbl_backlight_pwm_ops),       \
    .enable = _enable, .pwm = __VA_ARGS__,                                              \
  }

/** @} */

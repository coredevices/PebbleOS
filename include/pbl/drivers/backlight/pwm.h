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
 * @code{.c}
 * PBL_BACKLIGHT_PWM_DEFINE(s_backlight, .enable = PBL_GPIO(NRF5_GPIO_P1, 8, 0),
 *                          .pwm = PBL_PWM(&s_pwm0.ctrl, 0, PBL_PWM_HZ(256), 0),
 *                          .max_duty_cycle_percent = 67);
 *
 * const struct pbl_backlight *const BACKLIGHT = &s_backlight.backlight;
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
  /** Duty cycle at full brightness, in percent. */
  uint8_t max_duty_cycle_percent;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_backlight_pwm_ops;
/** @endcond */

/**
 * @brief Define a PWM backlight.
 *
 * @param sym Symbol of the instance; the backlight is @c sym.backlight.
 * @param ... Designated initializers of the remaining fields: @c enable, @c pwm and
 *            @c max_duty_cycle_percent.
 */
#define PBL_BACKLIGHT_PWM_DEFINE(sym, ...)                                                  \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                                          \
  const struct pbl_backlight_pwm sym = {                                                    \
    .backlight = PBL_BACKLIGHT_INIT(sym, "backlight_pwm", nullptr, &pbl_backlight_pwm_ops), \
    __VA_ARGS__                                                                             \
  };                                                                                        \
  PBL_DEVICE_REGISTER(sym, &sym.backlight.dev)

/** @} */

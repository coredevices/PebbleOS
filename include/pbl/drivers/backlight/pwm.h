/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/gpio.h>

/**
 * @defgroup drivers_backlight_pwm PWM backlight
 * @ingroup drivers_backlight
 * @brief @ref drivers_backlight implementation driving the LED with a PWM output.
 * @{
 */

/** @brief PWM backlight configuration. */
typedef struct {
  /** Optional enable output, active while the backlight is on. */
  const struct pbl_gpio ctl;
  /** PWM output. */
  const PwmConfig pwm;
  /** Duty cycle at full brightness, in percent. */
  uint8_t max_duty_cycle_percent;
} BacklightPwmConfig;

/** @} */
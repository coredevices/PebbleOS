/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_backlight_pwm PWM backlight
 * @ingroup drivers_backlight
 * @brief @ref drivers_backlight implementation driving the LED with a PWM output.
 * @{
 */

/** @brief PWM backlight configuration. */
typedef struct {
  /** Optional enable output, asserted while the backlight is on; unused if its gpio is NULL. */
  const OutputConfig ctl;
  /** PWM output. */
  const PwmConfig pwm;
  /** Duty cycle at full brightness, in percent. */
  uint8_t max_duty_cycle_percent;
} BacklightPwmConfig;

/** @} */
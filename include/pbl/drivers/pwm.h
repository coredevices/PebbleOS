/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <board/board.h>

/**
 * @defgroup drivers_pwm PWM
 * @ingroup drivers
 * @brief Pulse-width modulation outputs.
 *
 * A channel is described by a board-defined @c PwmConfig. The period is @p resolution counts
 * of a counter running at @p frequency, so the output frequency is frequency / resolution.
 *
 * @code{.c}
 * // 256 Hz output with 1024 steps
 * pwm_init(&BACKLIGHT_PWM.pwm, 1024, 1024 * 256);
 * pwm_enable(&BACKLIGHT_PWM.pwm, true);
 * pwm_set_duty_cycle(&BACKLIGHT_PWM.pwm, 512);
 * @endcode
 * @{
 */

/**
 * @brief Initialize a PWM channel.
 *
 * @param pwm Channel.
 * @param resolution Counts per period; duty cycles range from 0 to this value.
 * @param frequency Counter frequency in Hz.
 */
void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency);

/**
 * @brief Set the duty cycle.
 *
 * The channel must be enabled first.
 *
 * @param pwm Channel.
 * @param duty_cycle High time in counts, 0 to the resolution given to pwm_init().
 */
void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle);

/**
 * @brief Start or stop the output.
 *
 * @param pwm Channel.
 * @param enable true to start, false to stop.
 */
void pwm_enable(const PwmConfig *pwm, bool enable);

/** @} */

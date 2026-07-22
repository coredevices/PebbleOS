/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board/board.h"

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency);

// Note: pwm peripheral needs to be enabled before the duty cycle can be set
void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle);

void pwm_enable(const PwmConfig *pwm, bool enable);

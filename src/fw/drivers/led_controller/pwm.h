/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/pwm.h"
#include "drivers/led_controller.h"

typedef struct LedControllerPwm {
  PwmConfig pwm[3];
  uint32_t initial_color;
} LedControllerPwm;
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef struct {
  const OutputConfig ctl;
  const PwmConfig pwm;
  uint8_t max_duty_cycle_percent;
} BacklightPwmConfig;
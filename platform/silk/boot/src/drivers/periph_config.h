/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stm32f4xx.h"

typedef void (*ClockCmd)(uint32_t periph, FunctionalState state);

static inline void periph_config_enable(ClockCmd clock_cmd, uint32_t periph) {
  clock_cmd(periph, ENABLE);
}

static inline void periph_config_disable(ClockCmd clock_cmd, uint32_t periph) {
  clock_cmd(periph, DISABLE);
}

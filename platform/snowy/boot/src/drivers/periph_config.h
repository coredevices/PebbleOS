/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx.h"
#endif

typedef void (*ClockCmd)(uint32_t periph, FunctionalState state);

static inline void periph_config_init(void) {}
static inline void periph_config_acquire_lock(void) {}
static inline void periph_config_release_lock(void) {}

static inline void periph_config_enable(ClockCmd clock_cmd, uint32_t periph) {
  clock_cmd(periph, ENABLE);
}

static inline void periph_config_disable(ClockCmd clock_cmd, uint32_t periph) {
  clock_cmd(periph, DISABLE);
}

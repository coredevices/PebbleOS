/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

typedef enum {
  ExtiTrigger_Rising,
  ExtiTrigger_Falling,
  ExtiTrigger_RisingFalling
} ExtiTrigger;

typedef void (*ExtiHandlerCallback)(bool *should_context_switch);

#ifdef MICRO_FAMILY_STM32F4
//! See section 12.2.5 "External interrupt/event line mapping" in the STM32F2 reference manual
typedef enum {
  ExtiLineOther_RTCAlarm = 17,
  ExtiLineOther_RTCWakeup = 22
} ExtiLineOther;

//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger);

void exti_clear_pending_other(ExtiLineOther exti_line);
void exti_set_pending(ExtiConfig cfg);

void exti_enable_other(ExtiLineOther exti_line);
void exti_disable_other(ExtiLineOther exti_line);

static inline void exti_enable(ExtiConfig config) {
  exti_enable_other(config.exti_line);
}

static inline void exti_disable(ExtiConfig config) {
  exti_disable_other(config.exti_line);
}
#else
void exti_enable(ExtiConfig config);
void exti_disable(ExtiConfig config);
#endif

//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);
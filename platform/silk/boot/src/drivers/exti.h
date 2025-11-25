/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

typedef enum {
  ExtiTrigger_Rising,
  ExtiTrigger_Falling,
  ExtiTrigger_RisingFalling
} ExtiTrigger;

//! See section 12.2.5 "External interrupt/event line mapping" in the STM32F2 reference manual
typedef enum {
  ExtiLineOther_RTCAlarm = 17,
  ExtiLineOther_RTCWakeup = 22
} ExtiLineOther;

typedef void (*ExtiHandlerCallback)(void);

//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);
//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger);

static inline void exti_enable(ExtiConfig config);
static inline void exti_disable(ExtiConfig config);

void exti_enable_other(ExtiLineOther);
void exti_disable_other(ExtiLineOther);

#include "exti.inl.h"

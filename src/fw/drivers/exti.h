/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "board/board.h"

//! For simplicity we just configure all our EXTI-related interrupts to the same priority. This
//! way we don't have to worry about different lines wanting differing priorities when they share
//! the same NVIC channel (and therefore the same priority)
#define EXTI_PRIORITY (0x0e)

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

typedef void (*ExtiHandlerCallback)(bool *should_context_switch);

//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);
//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger);

#if !defined(MICRO_FAMILY_NRF5) && !defined(MICRO_FAMILY_SF32LB52)
static inline void exti_enable(ExtiConfig config);
static inline void exti_disable(ExtiConfig config);
#else
void exti_enable(ExtiConfig config);
void exti_disable(ExtiConfig config);
#endif

void exti_enable_other(ExtiLineOther exti_line);
void exti_disable_other(ExtiLineOther exti_line);

void exti_set_pending(ExtiConfig cfg);

void exti_clear_pending_other(ExtiLineOther exti_line);

#include "exti.inl.h"


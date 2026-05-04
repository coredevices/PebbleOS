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

void exti_enable(ExtiConfig config);
void exti_disable(ExtiConfig config);

//! Configures the given EXTI and NVIC for the given configuration.
void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);
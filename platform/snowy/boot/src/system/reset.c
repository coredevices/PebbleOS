/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/reset.h"

#include "drivers/display.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx.h"
#endif

void system_reset(void) {
  display_prepare_for_reset();
  system_hard_reset();
}

void system_hard_reset(void) {
  NVIC_SystemReset();
  __builtin_unreachable();
}

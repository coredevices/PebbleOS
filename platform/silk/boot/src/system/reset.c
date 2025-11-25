/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/reset.h"

#include "drivers/display.h"

#include "stm32f4xx.h"

void system_reset(void) {
  display_prepare_for_reset();

  // Clear the reset reason since it will no longer
  // apply after this bootloader reset
  RCC_ClearFlag();

  system_hard_reset();
}

void system_hard_reset(void) {
  NVIC_SystemReset();
  __builtin_unreachable();
}

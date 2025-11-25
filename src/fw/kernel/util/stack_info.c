/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "FreeRTOS.h"
#include "task.h"

#include "mcu/interrupts.h"

extern uint32_t __isr_stack_start__[];

uint32_t stack_free_bytes(void) {

  // Get the current SP
  register uint32_t SP __asm ("sp");
  uint32_t cur_sp = SP;

  // Default stack
  uint32_t start = (uint32_t) __isr_stack_start__;

  // On ISR stack?
  if (!mcu_state_is_isr()) {
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    if (task_handle != NULL) {
      // task_handle is NULL before we start the first task
      start = (uint32_t)ulTaskGetStackStart(task_handle);
    }
  }

  return cur_sp - start;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/debug.h>
#include <pbl/mcu/interrupts.h>

extern uint32_t __isr_stack_start__[];

uint32_t stack_free_bytes(void) {
  // The current SP, give or take a few bytes
  uint8_t marker;
  uintptr_t cur_sp = (uintptr_t)&marker;

  // Default stack
  uintptr_t start = (uintptr_t)__isr_stack_start__;

  // On ISR stack?
  if (!mcu_state_is_isr()) {
    struct pbl_thread *thread = pbl_thread_current();
    if (thread != nullptr) {
      // NULL before the first thread starts
      struct pbl_thread_stack_info info;
      pbl_thread_stack_info(thread, &info);
      start = (uintptr_t)info.start;
    }
  }

  return cur_sp - start;
}

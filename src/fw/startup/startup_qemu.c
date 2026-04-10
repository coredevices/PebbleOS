/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "util/attributes.h"

//! These symbols are defined in the linker script for use in initializing
//! the data sections.
extern uint8_t __data_load_start[];
extern uint8_t __data_start[];
extern uint8_t __data_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
extern uint8_t __isr_stack_start__[];

extern int main(void);

NORETURN prv_startup(void) {
  // Copy data section from flash to RAM
  for (int i = 0; i < (__data_end - __data_start); i++) {
    __data_start[i] = __data_load_start[i];
  }

  // Clear the bss section
  memset(__bss_start, 0, __bss_end - __bss_start);

  main();

  while (1) {}
}

NAKED_FUNC NORETURN Reset_Handler(void) {
  __asm volatile(
    // Set MSPLIM to protect the ISR stack (Cortex-M33)
    "ldr r0, =__isr_stack_start__ \n"
    "msr msplim, r0               \n"
    // Clear PSPLIM - set per-task by FreeRTOS
    "mov r0, #0                   \n"
    "msr psplim, r0               \n"
    // Jump to C startup
    "b prv_startup                \n"
  );
}

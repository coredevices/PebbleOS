/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <kernel.h>

PBL_NAKED PBL_NORETURN void Reset_Handler(void) {
  __asm volatile(
#ifdef CONFIG_CPU_CORTEX_M_HAS_SPLIM
      "  ldr r0, =__isr_stack_start__ \n"
      "  msr msplim, r0 \n"
      "  mov r0, #0 \n"
      "  msr psplim, r0 \n"
#endif
      "  b kernel_prep_c \n"
      "  .ltorg \n");
}

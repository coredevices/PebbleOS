/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <pbl/kernel/compiler.h>

#include <arch.h>
#include <kernel.h>

[[noreturn]] PBL_NAKED void Reset_Handler(void) {
  __asm volatile(
#ifdef CONFIG_CPU_CORTEX_M_HAS_SPLIM
#ifdef CONFIG_KERNEL_STACK_GUARD
      "  ldr r0, =__isr_stack_start__ \n"
#else
      "  mov r0, #0 \n"
#endif
      "  msr msplim, r0 \n"
      "  mov r0, #0 \n"
      "  msr psplim, r0 \n"
#endif
      "  b kernel_prep_c \n"
      "  .ltorg \n");
}

[[noreturn]] void arch_reset(void) {
  arm_dsb();
  arch_writel(ARM_SCB_AIRCR_VECTKEY | (arch_readl(ARM_SCB_AIRCR) & ARM_SCB_AIRCR_PRIGROUP_MSK) |
                  ARM_SCB_AIRCR_SYSRESETREQ,
              ARM_SCB_AIRCR);
  arm_dsb();
  for (;;) {
  }
}

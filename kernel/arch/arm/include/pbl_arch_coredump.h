/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * Assembly every fault handler runs first, while r4-r11 still hold the faulting code's values, so
 * a core dump can show that code as it was. Uses r0-r3.
 */
#ifdef CONFIG_COREDUMP
#define PBL_ARCH_COREDUMP_FAULT_ENTRY \
  "ldr r0, =arch_coredump_fault\n"    \
  "stmia r0!, {r4-r11, lr}\n"         \
  "mrs r1, msp\n"                     \
  "mrs r2, psp\n"                     \
  "ldr r3, =0xE000ED28\n" /* CFSR */  \
  "ldr r3, [r3]\n"                    \
  "stmia r0, {r1-r3}\n"
#else
#define PBL_ARCH_COREDUMP_FAULT_ENTRY ""
#endif

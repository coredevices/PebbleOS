/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <pbl_arch_csr.h>

#ifdef CONFIG_RISCV_WFI_NEEDS_MIE
// The core only wakes from an interrupt taken with interrupts unmasked: one taken before the
// wfi runs, and the sleep lasts until the next one
static PBL_ALWAYS_INLINE void arch_cpu_idle(void) {
  const unsigned long mstatus = arch_csr_read(ARCH_CSR_MSTATUS);

  __asm volatile("fence rw, rw\n\tcsrsi mstatus, %0\n\twfi" ::"i"(ARCH_MSTATUS_MIE) : "memory");
  arch_csr_clear(ARCH_CSR_MSTATUS, ~mstatus & ARCH_MSTATUS_MIE);
}
#else
static PBL_ALWAYS_INLINE void arch_cpu_idle(void) {
  __asm volatile("fence rw, rw\n\twfi" ::: "memory");
}
#endif

#define arch_cpu_idle_deep()       arch_cpu_idle()
#define arch_cpu_wait_event_deep() arch_cpu_idle()
#define arch_cpu_send_event()      ((void)0)

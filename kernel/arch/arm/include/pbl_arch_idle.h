/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

#define ARCH_SCB_SCR           0xE000ED10UL
#define ARCH_SCB_SCR_SLEEPDEEP (1UL << 2)

static PBL_ALWAYS_INLINE void arch_cpu_idle(void) {
  __asm volatile("dsb 0xf" ::: "memory");
  __asm volatile("wfi" ::: "memory");
  __asm volatile("isb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void arch_cpu_idle_deep(void) {
  arch_writel(arch_readl(ARCH_SCB_SCR) | ARCH_SCB_SCR_SLEEPDEEP, ARCH_SCB_SCR);
  arch_cpu_idle();
  arch_writel(arch_readl(ARCH_SCB_SCR) & ~ARCH_SCB_SCR_SLEEPDEEP, ARCH_SCB_SCR);
}

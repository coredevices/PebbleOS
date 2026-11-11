/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <pbl/kernel/cache.h>

void pbl_icache_invalidate_all(void) {
  arm_dsb();
  arm_isb();
  arch_writel(0U, ARM_ICIALLU);
  arm_dsb();
  arm_isb();
}

void pbl_icache_enable(void) {
  if ((arch_readl(ARM_SCB_CCR) & ARM_SCB_CCR_IC) != 0U) {
    return;
  }

  pbl_icache_invalidate_all();
  arch_writel(arch_readl(ARM_SCB_CCR) | ARM_SCB_CCR_IC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();
}

void pbl_icache_disable(void) {
  arm_dsb();
  arm_isb();
  arch_writel(arch_readl(ARM_SCB_CCR) & ~ARM_SCB_CCR_IC, ARM_SCB_CCR);
  pbl_icache_invalidate_all();
}

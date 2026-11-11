/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <stdint.h>

#include <pbl/kernel/cache.h>

static_assert((1U << ARM_DCSW_SET_SHIFT) == CONFIG_DCACHE_LINE_SIZE,
              "set/way operations assume the line size");

static void prv_all(uintptr_t reg) {
  arch_writel(ARM_CSSELR_L1_DCACHE, ARM_SCB_CSSELR);
  arm_dsb();

  const uint32_t ccsidr = arch_readl(ARM_SCB_CCSIDR);
  uint32_t sets = ARM_CCSIDR_SETS(ccsidr);

  do {
    uint32_t ways = ARM_CCSIDR_WAYS(ccsidr);

    do {
      arch_writel(ARM_DCSW(sets, ways), reg);
    } while (ways-- != 0U);
  } while (sets-- != 0U);

  arm_dsb();
  arm_isb();
}

void pbl_dcache_enable(void) {
  if ((arch_readl(ARM_SCB_CCR) & ARM_SCB_CCR_DC) != 0U) {
    return;
  }

  prv_all(ARM_DCISW);
  arch_writel(arch_readl(ARM_SCB_CCR) | ARM_SCB_CCR_DC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();
}

void pbl_dcache_disable(void) {
  arch_writel(arch_readl(ARM_SCB_CCR) & ~ARM_SCB_CCR_DC, ARM_SCB_CCR);
  arm_dsb();
  prv_all(ARM_DCCISW);
}

void pbl_dcache_flush_all(void) {
  prv_all(ARM_DCCSW);
}

void pbl_dcache_invalidate_all(void) {
  prv_all(ARM_DCISW);
}

void pbl_dcache_flush_invalidate_all(void) {
  prv_all(ARM_DCCISW);
}

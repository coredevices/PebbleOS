/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>
#include <pbl/util/testing.h>

#ifdef CONFIG_CPU_HAS_ICACHE
static uint32_t s_icache_cssidr;
#endif

#ifdef CONFIG_CPU_HAS_DCACHE
static uint32_t s_dcache_cssidr;
#endif

#if defined(CONFIG_CPU_HAS_ICACHE) || defined(CONFIG_CPU_HAS_DCACHE)
static uint32_t prv_get_line_size(uint32_t ccsidr) {
  return (ARM_CCSIDR_LINESIZE(ccsidr) + 1) << 4;
}

static void prv_cache_operation_range(uintptr_t reg, uint32_t line_size, uintptr_t addr,
                                      size_t size) {
  intptr_t op_size = size;

  arm_dsb();
  arm_isb();

  while (op_size > 0) {
    arch_writel(addr, reg);
    addr += line_size;
    op_size -= line_size;
  }

  arm_dsb();
  arm_isb();
}
#endif

#ifdef CONFIG_CPU_HAS_DCACHE
// Reads the geometry itself: the cache may have been enabled before dcache_enable().
static void prv_dcache_operation_all(uintptr_t reg) {
  uint32_t ccsidr;
  uint32_t sets;
  uint32_t ways;

  arch_writel(ARM_CSSELR_L1_DCACHE, ARM_SCB_CSSELR);
  arm_dsb();
  ccsidr = arch_readl(ARM_SCB_CCSIDR);

  sets = ARM_CCSIDR_SETS(ccsidr);
  do {
    ways = ARM_CCSIDR_WAYS(ccsidr);
    do {
      arch_writel(ARM_DCSW(sets, ways), reg);
    } while (ways--);
  } while (sets--);

  arm_dsb();
  arm_isb();
}
#endif

PBL_T_MOCKABLE void icache_enable(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  arch_writel(ARM_CSSELR_L1_ICACHE, ARM_SCB_CSSELR);
  arm_dmb();
  s_icache_cssidr = arch_readl(ARM_SCB_CCSIDR);

  icache_invalidate_all();

  arm_dsb();
  arm_isb();
  arch_writel(arch_readl(ARM_SCB_CCR) | ARM_SCB_CCR_IC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();
#endif
}

PBL_T_MOCKABLE void icache_disable(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  arm_dsb();
  arm_isb();
  arch_writel(arch_readl(ARM_SCB_CCR) & ~ARM_SCB_CCR_IC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();

  icache_invalidate_all();
#endif
}

PBL_T_MOCKABLE bool icache_is_enabled(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  return (arch_readl(ARM_SCB_CCR) & ARM_SCB_CCR_IC) != 0U;
#endif
  return false;
}

PBL_T_MOCKABLE uint32_t icache_line_size(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  return prv_get_line_size(s_icache_cssidr);
#endif
  return 1;
}

PBL_T_MOCKABLE void dcache_enable(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  arch_writel(ARM_CSSELR_L1_DCACHE, ARM_SCB_CSSELR);
  arm_dmb();
  s_dcache_cssidr = arch_readl(ARM_SCB_CCSIDR);

  dcache_invalidate_all();
  arm_dsb();
  arch_writel(arch_readl(ARM_SCB_CCR) | ARM_SCB_CCR_DC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();
#endif
}

PBL_T_MOCKABLE void dcache_disable(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  dcache_flush_invalidate_all();
  arm_dsb();
  arch_writel(arch_readl(ARM_SCB_CCR) & ~ARM_SCB_CCR_DC, ARM_SCB_CCR);
  arm_dsb();
  arm_isb();
#endif
}

PBL_T_MOCKABLE bool dcache_is_enabled(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  return (arch_readl(ARM_SCB_CCR) & ARM_SCB_CCR_DC) != 0U;
#endif
  return false;
}

PBL_T_MOCKABLE uint32_t dcache_line_size(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  return prv_get_line_size(s_dcache_cssidr);
#endif
  return 1;
}

PBL_T_MOCKABLE uint32_t dcache_alignment_mask_minimum(uint32_t min) {
#ifdef CONFIG_CPU_HAS_DCACHE
  const uint32_t line_size = dcache_line_size();
  if (line_size > min) {
    return line_size - 1;
  }
#endif
  return min - 1;
}

PBL_T_MOCKABLE void icache_invalidate_all(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  arm_dsb();
  arm_isb();
  arch_writel(0U, ARM_ICIALLU);
  arm_dsb();
  arm_isb();
#endif
}

PBL_T_MOCKABLE void icache_invalidate(void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_ICACHE
  prv_cache_operation_range(ARM_ICIMVAU, icache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_flush_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(ARM_DCCSW);
#endif
}

PBL_T_MOCKABLE void dcache_invalidate_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(ARM_DCISW);
#endif
}

PBL_T_MOCKABLE void dcache_flush_invalidate_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(ARM_DCCISW);
#endif
}

PBL_T_MOCKABLE void dcache_flush(const void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(ARM_DCCMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_invalidate(void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(ARM_DCIMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_flush_invalidate(const void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(ARM_DCCIMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

static void prv_align(uintptr_t *addr, size_t *size, uint32_t line_size) {
  uint32_t line_mask = line_size - 1;
  if (*addr & line_mask) {
    // Need to adjust the address and size because the address is unaligned.
    *size += *addr & line_mask;
    *addr &= ~line_mask;
  }
  if (*size & line_mask) {
    // Need to round the size up because the size is unaligned.
    *size = (*size + line_mask) & ~line_mask;
  }
}

void icache_align(uintptr_t *addr, size_t *size) {
  prv_align(addr, size, icache_line_size());
}

void dcache_align(uintptr_t *addr, size_t *size) {
  prv_align(addr, size, dcache_line_size());
}

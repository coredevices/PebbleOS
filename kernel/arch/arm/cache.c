/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>
#include <pbl/util/testing.h>

#include <cmsis_core.h>

// Most of these implementations are derived from CMSIS
#define CCSIDR_LINESIZE(x) (((x) & SCB_CCSIDR_LINESIZE_Msk) >> SCB_CCSIDR_LINESIZE_Pos)
#define CCSIDR_WAYS(x)     (((x) & SCB_CCSIDR_ASSOCIATIVITY_Msk) >> SCB_CCSIDR_ASSOCIATIVITY_Pos)

#define CSSELR_L1_DCACHE 0
#define CSSELR_L1_ICACHE 1

#ifdef CONFIG_CPU_HAS_ICACHE
static uint32_t s_icache_cssidr;
#endif

#ifdef CONFIG_CPU_HAS_DCACHE
static uint32_t s_dcache_cssidr;
#endif

#if defined(CONFIG_CPU_HAS_ICACHE) || defined(CONFIG_CPU_HAS_DCACHE)
static uint32_t prv_get_line_size(uint32_t ccsidr) {
  return ((CCSIDR_LINESIZE(ccsidr)) + 1) << 4;
}

static void prv_cache_operation_range(volatile uint32_t *reg, uint32_t line_size, uintptr_t addr,
                                      size_t size) {
  intptr_t op_size = size;

  __DSB();
  __ISB();

  while (op_size > 0) {
    *reg = addr;
    addr += line_size;
    op_size -= line_size;
  }

  __DSB();
  __ISB();
}
#endif

#ifdef CONFIG_CPU_HAS_DCACHE
static void prv_dcache_operation_all(volatile uint32_t *reg) {
  uint32_t ccsidr;
  uint32_t sets;
  uint32_t ways;

  ccsidr = s_dcache_cssidr;

  sets = CCSIDR_SETS(ccsidr);
  do {
    ways = CCSIDR_WAYS(ccsidr);
    do {
      *reg = (((sets << SCB_DCISW_SET_Pos) & SCB_DCISW_SET_Msk) |
              ((ways << SCB_DCISW_WAY_Pos) & SCB_DCISW_WAY_Msk));
#if defined(__CC_ARM)
      __schedule_barrier();
#endif
    } while (ways--);
  } while (sets--);

  __DSB();
  __ISB();
}
#endif

PBL_T_MOCKABLE void icache_enable(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  SCB->CSSELR = CSSELR_L1_ICACHE;
  __DMB();
  s_icache_cssidr = SCB->CCSIDR;

  icache_invalidate_all();

  __DSB();
  __ISB();
  SCB->CCR |= SCB_CCR_IC_Msk; // enable I-Cache
  __DSB();
  __ISB();
#endif
}

PBL_T_MOCKABLE void icache_disable(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  __DSB();
  __ISB();
  SCB->CCR &= ~SCB_CCR_IC_Msk; // disable I-Cache
  __DSB();
  __ISB();

  icache_invalidate_all();
#endif
}

PBL_T_MOCKABLE bool icache_is_enabled(void) {
#ifdef CONFIG_CPU_HAS_ICACHE
  return SCB->CCR & SCB_CCR_IC_Msk;
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
  SCB->CSSELR = CSSELR_L1_DCACHE;
  __DMB();
  s_dcache_cssidr = SCB->CCSIDR;

  dcache_invalidate_all();
  __DSB();
  SCB->CCR |= SCB_CCR_DC_Msk; // enable D-Cache
  __DSB();
  __ISB();
#endif
}

PBL_T_MOCKABLE void dcache_disable(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  dcache_flush_invalidate_all();
  __DSB();
  SCB->CCR &= ~SCB_CCR_DC_Msk; // disable D-Cache
  __DSB();
  __ISB();
#endif
}

PBL_T_MOCKABLE bool dcache_is_enabled(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  return SCB->CCR & SCB_CCR_DC_Msk;
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
  __DSB();
  __ISB();
  SCB->ICIALLU = 0;
  __DSB();
  __ISB();
#endif
}

PBL_T_MOCKABLE void icache_invalidate(void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_ICACHE
  prv_cache_operation_range(&SCB->ICIMVAU, icache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_flush_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(&SCB->DCCSW);
#endif
}

PBL_T_MOCKABLE void dcache_invalidate_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(&SCB->DCISW);
#endif
}

PBL_T_MOCKABLE void dcache_flush_invalidate_all(void) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_dcache_operation_all(&SCB->DCCISW);
#endif
}

PBL_T_MOCKABLE void dcache_flush(const void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(&SCB->DCCMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_invalidate(void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(&SCB->DCIMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

PBL_T_MOCKABLE void dcache_flush_invalidate(const void *addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  prv_cache_operation_range(&SCB->DCCIMVAC, dcache_line_size(), (uintptr_t)addr, size);
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

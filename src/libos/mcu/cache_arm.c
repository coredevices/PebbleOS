/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mcu/cache.h"
#include "util/attributes.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

// I-Cache definition doesn't always exist
#ifndef __ICACHE_PRESENT
# define __ICACHE_PRESENT 0U
#endif

// D-Cache definition doesn't always exist
#ifndef __DCACHE_PRESENT
# define __DCACHE_PRESENT 0U
#endif

// Most of these implementations are derived from CMSIS
#define CCSIDR_LINESIZE(x) (((x) & SCB_CCSIDR_LINESIZE_Msk) >> SCB_CCSIDR_LINESIZE_Pos)
#define CCSIDR_WAYS(x)     (((x) & SCB_CCSIDR_ASSOCIATIVITY_Msk) >> SCB_CCSIDR_ASSOCIATIVITY_Pos)

#define CSSELR_L1_DCACHE 0
#define CSSELR_L1_ICACHE 1

#if __ICACHE_PRESENT
static uint32_t s_icache_cssidr;
#endif

#if __DCACHE_PRESENT
static uint32_t s_dcache_cssidr;
#endif

#if __ICACHE_PRESENT || __DCACHE_PRESENT
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

#if __DCACHE_PRESENT
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

MOCKABLE void icache_enable(void) {
#if __ICACHE_PRESENT
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

MOCKABLE void icache_disable(void) {
#if __ICACHE_PRESENT
  __DSB();
  __ISB();
  SCB->CCR &= ~SCB_CCR_IC_Msk; // disable I-Cache
  __DSB();
  __ISB();

  icache_invalidate_all();
#endif
}

MOCKABLE bool icache_is_enabled(void) {
#if __ICACHE_PRESENT
  return SCB->CCR & SCB_CCR_IC_Msk;
#endif
  return false;
}

MOCKABLE uint32_t icache_line_size(void) {
#if __ICACHE_PRESENT
  return prv_get_line_size(s_icache_cssidr);
#endif
  return 1;
}


MOCKABLE void dcache_enable(void) {
#if __DCACHE_PRESENT
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

MOCKABLE void dcache_disable(void) {
#if __DCACHE_PRESENT
  dcache_flush_invalidate_all();
  __DSB();
  SCB->CCR &= ~SCB_CCR_DC_Msk; // disable D-Cache
  __DSB();
  __ISB();
#endif
}

MOCKABLE bool dcache_is_enabled(void) {
#if __DCACHE_PRESENT
  return SCB->CCR & SCB_CCR_DC_Msk;
#endif
  return false;
}

MOCKABLE uint32_t dcache_line_size(void) {
#if __DCACHE_PRESENT
  return prv_get_line_size(s_dcache_cssidr);
#endif
  return 1;
}

MOCKABLE uint32_t dcache_alignment_mask_minimum(uint32_t min) {
#if __DCACHE_PRESENT
  const uint32_t line_size = dcache_line_size();
  if (line_size > min) {
    return line_size - 1;
  }
#endif
  return min - 1;
}

MOCKABLE void icache_invalidate_all(void) {
#if __ICACHE_PRESENT
  __DSB();
  __ISB();
  SCB->ICIALLU = 0;
  __DSB();
  __ISB();
#endif
}

MOCKABLE void icache_invalidate(void *addr, size_t size) {
#if __ICACHE_PRESENT
  prv_cache_operation_range(&SCB->ICIMVAU, icache_line_size(), (uintptr_t)addr, size);
#endif
}

MOCKABLE void dcache_flush_all(void) {
#if __DCACHE_PRESENT
  prv_dcache_operation_all(&SCB->DCCSW);
#endif
}

MOCKABLE void dcache_invalidate_all(void) {
#if __DCACHE_PRESENT
  prv_dcache_operation_all(&SCB->DCISW);
#endif
}

MOCKABLE void dcache_flush_invalidate_all(void) {
#if __DCACHE_PRESENT
  prv_dcache_operation_all(&SCB->DCCISW);
#endif
}

MOCKABLE void dcache_flush(const void *addr, size_t size) {
#if __DCACHE_PRESENT
  prv_cache_operation_range(&SCB->DCCMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

MOCKABLE void dcache_invalidate(void *addr, size_t size) {
#if __DCACHE_PRESENT
  prv_cache_operation_range(&SCB->DCIMVAC, dcache_line_size(), (uintptr_t)addr, size);
#endif
}

MOCKABLE void dcache_flush_invalidate(const void *addr, size_t size) {
#if __DCACHE_PRESENT
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

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

#define ARCH_ICIMVAU  0xE000EF58UL
#define ARCH_DCIMVAC  0xE000EF5CUL
#define ARCH_DCCMVAC  0xE000EF68UL
#define ARCH_DCCIMVAC 0xE000EF70UL

#ifdef CONFIG_CPU_HAS_DCACHE
#define ARCH_DCACHE_LINE_SIZE CONFIG_DCACHE_LINE_SIZE
#else
#define ARCH_DCACHE_LINE_SIZE 1U
#endif

#ifdef CONFIG_CPU_HAS_ICACHE
#define ARCH_ICACHE_LINE_SIZE CONFIG_ICACHE_LINE_SIZE
#else
#define ARCH_ICACHE_LINE_SIZE 1U
#endif

// Writes the address of every line [addr, addr + size) touches to the maintenance register reg.
static PBL_ALWAYS_INLINE void arch_cache_lines(uintptr_t reg, uintptr_t addr, size_t size,
                                               uintptr_t line_size) {
  const uintptr_t end = addr + size;

  __asm volatile("dsb 0xf" ::: "memory");
  for (uintptr_t line = addr & ~(line_size - 1U); line < end; line += line_size) {
    arch_writel(line, reg);
  }
  __asm volatile("dsb 0xf\n\tisb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void arch_dcache_flush_range(uintptr_t addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  arch_cache_lines(ARCH_DCCMVAC, addr, size, ARCH_DCACHE_LINE_SIZE);
#else
  (void)addr;
  (void)size;
#endif
}

static PBL_ALWAYS_INLINE void arch_dcache_invalidate_range(uintptr_t addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  arch_cache_lines(ARCH_DCIMVAC, addr, size, ARCH_DCACHE_LINE_SIZE);
#else
  (void)addr;
  (void)size;
#endif
}

static PBL_ALWAYS_INLINE void arch_dcache_flush_invalidate_range(uintptr_t addr, size_t size) {
#ifdef CONFIG_CPU_HAS_DCACHE
  arch_cache_lines(ARCH_DCCIMVAC, addr, size, ARCH_DCACHE_LINE_SIZE);
#else
  (void)addr;
  (void)size;
#endif
}

static PBL_ALWAYS_INLINE void arch_icache_invalidate_range(uintptr_t addr, size_t size) {
#ifdef CONFIG_CPU_HAS_ICACHE
  arch_cache_lines(ARCH_ICIMVAU, addr, size, ARCH_ICACHE_LINE_SIZE);
#else
  (void)addr;
  (void)size;
#endif
}

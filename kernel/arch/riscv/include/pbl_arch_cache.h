/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

// No caches to maintain: the core reaches memory directly

#define ARCH_DCACHE_LINE_SIZE 1U
#define ARCH_ICACHE_LINE_SIZE 1U

static PBL_ALWAYS_INLINE void arch_dcache_flush_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static PBL_ALWAYS_INLINE void arch_dcache_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static PBL_ALWAYS_INLINE void arch_dcache_flush_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static PBL_ALWAYS_INLINE void arch_icache_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
  __asm volatile("fence.i" ::: "memory");
}

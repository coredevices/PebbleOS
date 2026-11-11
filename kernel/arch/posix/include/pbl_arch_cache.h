/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// The host keeps its caches coherent.

#define ARCH_DCACHE_LINE_SIZE 1U
#define ARCH_ICACHE_LINE_SIZE 1U

static inline void arch_dcache_flush_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static inline void arch_dcache_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static inline void arch_dcache_flush_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static inline void arch_icache_invalidate_range(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}

static inline void arch_dcache_invalidate_lines(uintptr_t addr, size_t size, uint32_t line_size) {
  (void)addr;
  (void)size;
  (void)line_size;
}

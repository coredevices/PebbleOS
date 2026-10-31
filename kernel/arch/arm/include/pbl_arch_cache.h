/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

#define ARCH_SCB_DCIMVAC 0xE000EF5CUL

static PBL_ALWAYS_INLINE void arch_dcache_invalidate_lines(uintptr_t addr, size_t size,
                                                           uint32_t line_size) {
  for (uintptr_t line = addr & ~(uintptr_t)(line_size - 1U); line < addr + size;
       line += line_size) {
    arch_writel(line, ARCH_SCB_DCIMVAC);
  }

  __asm volatile("dsb 0xf\n\tisb 0xf" ::: "memory");
}

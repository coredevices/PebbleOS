/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

static inline void arch_dcache_invalidate_lines(uintptr_t addr, size_t size, uint32_t line_size) {
  (void)addr;
  (void)size;
  (void)line_size;
}

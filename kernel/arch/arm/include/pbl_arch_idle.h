/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

static PBL_ALWAYS_INLINE void arch_cpu_idle(void) {
  __asm volatile("dsb 0xf" ::: "memory");
  __asm volatile("wfi" ::: "memory");
  __asm volatile("isb 0xf" ::: "memory");
}

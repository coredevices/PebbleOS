/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

static PBL_ALWAYS_INLINE uint8_t arch_readb(uintptr_t addr) {
  return *(const volatile uint8_t *)addr;
}

static PBL_ALWAYS_INLINE uint16_t arch_readw(uintptr_t addr) {
  return *(const volatile uint16_t *)addr;
}

static PBL_ALWAYS_INLINE uint32_t arch_readl(uintptr_t addr) {
  return *(const volatile uint32_t *)addr;
}

static PBL_ALWAYS_INLINE void arch_writeb(uint8_t val, uintptr_t addr) {
  *(volatile uint8_t *)addr = val;
}

static PBL_ALWAYS_INLINE void arch_writew(uint16_t val, uintptr_t addr) {
  *(volatile uint16_t *)addr = val;
}

static PBL_ALWAYS_INLINE void arch_writel(uint32_t val, uintptr_t addr) {
  *(volatile uint32_t *)addr = val;
}

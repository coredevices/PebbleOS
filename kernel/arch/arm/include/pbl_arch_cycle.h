/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

#define ARCH_DEMCR              0xE000EDFCUL
#define ARCH_DEMCR_TRCENA       (1UL << 24)
#define ARCH_DWT_CTRL           0xE0001000UL
#define ARCH_DWT_CYCCNT         0xE0001004UL
#define ARCH_DWT_CTRL_CYCCNTENA (1UL << 0)

// Enabled on first use and re-enabled if a low power mode or a debugger turned it off, either
// the counter or the trace block it belongs to. Inline throughout: code running from RAM while
// the flash is unreadable counts cycles too.
static PBL_ALWAYS_INLINE uint32_t arch_cycle_get_32(void) {
  if ((arch_readl(ARCH_DEMCR) & ARCH_DEMCR_TRCENA) == 0U ||
      (arch_readl(ARCH_DWT_CTRL) & ARCH_DWT_CTRL_CYCCNTENA) == 0U) {
    arch_writel(arch_readl(ARCH_DEMCR) | ARCH_DEMCR_TRCENA, ARCH_DEMCR);
    arch_writel(arch_readl(ARCH_DWT_CTRL) | ARCH_DWT_CTRL_CYCCNTENA, ARCH_DWT_CTRL);
  }
  return arch_readl(ARCH_DWT_CYCCNT);
}

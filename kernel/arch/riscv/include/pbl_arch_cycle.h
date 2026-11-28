/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_csr.h>

#ifdef CONFIG_RISCV_SOC_CYCLE_COUNTER
#include <soc_riscv.h>
#endif

static PBL_ALWAYS_INLINE uint32_t arch_cycle_get_32(void) {
#ifdef CONFIG_RISCV_SOC_CYCLE_COUNTER
  return soc_riscv_cycle_get();
#else
  return (uint32_t)arch_csr_read(ARCH_CSR_MCYCLE);
#endif
}

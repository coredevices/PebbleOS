/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! What the RISC-V arch needs from a Nordic VPR.

#include <hardware/nrf_vpr_csr.h>

//! VEVIF task that switches threads, the last one, and the CLIC line it drives.
#define SOC_RISCV_SWITCH_IRQ 22

#ifdef __ASSEMBLER__

// clang-format off
// Clears the switch, with reg as scratch
.macro soc_riscv_switch_clear reg
  li \reg, (1 << SOC_RISCV_SWITCH_IRQ)
  csrc NRF_VPR_CSR_TASKS, \reg
.endm
// clang-format on

#else

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_csr.h>

static PBL_ALWAYS_INLINE void soc_riscv_switch_raise(void) {
  arch_csr_set(NRF_VPR_CSR_TASKS, 1U << SOC_RISCV_SWITCH_IRQ);
}

static PBL_ALWAYS_INLINE bool soc_riscv_switch_pending(void) {
  return (arch_csr_read(NRF_VPR_CSR_TASKS) & (1U << SOC_RISCV_SWITCH_IRQ)) != 0U;
}

// mcycle does not count: VTIM counts the cycles down instead
static PBL_ALWAYS_INLINE uint32_t soc_riscv_cycle_get(void) {
  return ~(uint32_t)arch_csr_read(NRF_VPR_CSR_CNT);
}

//! Enables the VEVIF and VTIM CSRs, and starts VTIM counting cycles.
void pbl_nrf_vpr_init(void);

#endif

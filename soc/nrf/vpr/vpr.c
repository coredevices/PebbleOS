/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl_arch_csr.h>
#include <soc_riscv.h>

void pbl_nrf_vpr_init(void) {
  // The VEVIF CSRs, which switch threads, and VTIM, which counts cycles, are real-time peripherals
  arch_csr_write(NRF_VPR_CSR_CTRL, (arch_csr_read(NRF_VPR_CSR_CTRL) & ~NRF_VPR_CTRL_KEY_MSK) |
                                       NRF_VPR_CTRL_KEY | NRF_VPR_CTRL_ENABLERTPERIPH);

  // One 32-bit counter, wrapping, which only counts once written
  arch_csr_write(NRF_VPR_CSR_CNTMODE0, NRF_VPR_CNTMODE_WRAP);
  arch_csr_write(NRF_VPR_CSR_CNTMODE1, NRF_VPR_CNTMODE_COMBINE);
  arch_csr_write(NRF_VPR_CSR_CNT, UINT32_MAX);
}

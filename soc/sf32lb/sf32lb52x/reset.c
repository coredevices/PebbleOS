/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdint.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/section.h>

#include <hardware/sf32lb52_hpsys_aon.h>
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_hpsys_rcc.h>
#include <hardware/sf32lb52_pmuc.h>

#define PMUC(reg) (SF32LB52_PMUC_BASE + SF32LB52_PMUC_##reg##_OFFSET)

#define HRC_CR_DEFAULT 0x0a50c015U

#define FLASH_LDO_OFF_US   5000U
#define FLASH_DISCHARGE_US 8000U
#define REBOOT_PULSE_US    100000U

// Letter series parts have no 3.3 V peripheral LDOs
static PBL_ALWAYS_INLINE bool prv_has_flash_ldo(void) {
  uint32_t rev =
      pbl_readl(SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_IDR_OFFSET) & HPSYS_CFG_IDR_REVID_MASK;

  return rev != HPSYS_CFG_IDR_REVID_A4 && rev != HPSYS_CFG_IDR_REVID_B4;
}

// Inline, unlike pbl_modifyl(): the flash goes away and interrupts are off
static PBL_ALWAYS_INLINE void prv_update(uint32_t clear, uint32_t set, uintptr_t addr) {
  pbl_writel((pbl_readl(addr) & ~clear) | set, addr);
}

// Powers the flash down, so that it boots back in 3-byte address mode
[[noreturn]] PBL_SECTION_RAM static void prv_reset(void) {
  const uintptr_t csr = SF32LB52_HPSYS_RCC_BASE + SF32LB52_HPSYS_RCC_CSR_OFFSET;
  const uintptr_t acr = SF32LB52_HPSYS_AON_BASE + SF32LB52_HPSYS_AON_ACR_OFFSET;

  if (prv_has_flash_ldo()) {
    prv_update(PMUC_PERI_LDO_EN_VDD33_LDO2, PMUC_PERI_LDO_VDD33_LDO2_PD, PMUC(PERI_LDO));
    pbl_busy_wait_us(FLASH_LDO_OFF_US);
  }
  pbl_busy_wait_us(FLASH_DISCHARGE_US);

  // The boot ROM expects the RC oscillator at its default rate
  if ((pbl_readl(csr) & HPSYS_RCC_CSR_SEL_SYS_MASK) == HPSYS_RCC_CSR_SEL_SYS_HRC48) {
    prv_update(0U, HPSYS_AON_ACR_HXT48_REQ, acr);
    while ((pbl_readl(acr) & HPSYS_AON_ACR_HXT48_RDY) == 0U) {
    }
    prv_update(HPSYS_RCC_CSR_SEL_SYS_MASK, HPSYS_RCC_CSR_SEL_SYS_HXT48, csr);
  }
  pbl_writel(HRC_CR_DEFAULT, PMUC(HRC_CR));

  // The PMU acts on the rising edge
  if ((pbl_readl(PMUC(CR)) & PMUC_CR_REBOOT) != 0U) {
    prv_update(PMUC_CR_REBOOT, 0U, PMUC(CR));
    pbl_busy_wait_us(REBOOT_PULSE_US);
  }
  prv_update(0U, PMUC_CR_REBOOT, PMUC(CR));

  while (true) {
  }
}

[[noreturn]] void pbl_soc_reset(void) {
  (void)pbl_irq_disable_all();

  soc_sf32lb52_lpsys_wake();
  soc_sf32lb52_lpsys_halt(true);

  prv_reset();
}

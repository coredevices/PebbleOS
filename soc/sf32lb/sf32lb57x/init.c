/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_rcc.h>
#include <hardware/sf32lb57_hpsys_aon.h>
#include <hardware/sf32lb57_hpsys_rcc.h>
#include <hardware/sf32lb57_lpsys_aon.h>
#include <hardware/sf32lb57_wdt.h>
#include <lpsys.h>

#define HPSYS_AON(reg) (PBL_DT_REG_HPSYS_AON + SF32LB57_HPSYS_AON_##reg##_OFFSET)
#define HPSYS_RCC(reg) (PBL_DT_REG_HPSYS_RCC + SF32LB52_HPSYS_RCC_##reg##_OFFSET)
#define WDT1(reg)      (PBL_DT_REG_WDT1 + SF32LB57_WDT_##reg##_OFFSET)

// Deep WFI clocks: HRC48, HCLK = 48 MHz / 12 = 4 MHz, PCLK1 = 4 MHz, PCLK2 = 2 MHz
#define DEEPWFI_HDIV  12U
#define DEEPWFI_PDIV1 0U
#define DEEPWFI_PDIV2 1U

#define SYSCLK_HZ   48000000U
#define DLL_STEP_HZ 24000000U

uint32_t SystemCoreClock = SYSCLK_HZ;

static uint32_t prv_hclk_hz(void) {
  uint32_t csr = pbl_readl(HPSYS_RCC(CSR));
  uint32_t hdiv = PBL_FIELD_GET(HPSYS_RCC_CFGR_HDIV_MASK, pbl_readl(HPSYS_RCC(CFGR)));
  uint32_t sys = SYSCLK_HZ;

  if (PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK, csr) == HPSYS_RCC_CSR_SEL_SYS_DLL1) {
    uint32_t dll = pbl_readl(HPSYS_RCC(DLL1CR));

    sys = (dll & HPSYS_RCC_DLLCR_EN) != 0U
              ? (PBL_FIELD_GET(HPSYS_RCC_DLLCR_STG_MASK, dll) + 1U) * DLL_STEP_HZ
              : 0U;
    if ((dll & SF32LB57_HPSYS_RCC_DLLCR_OUT_DIV2_EN) != 0U) {
      sys /= 2U;
    }
  }

  return sys / (hdiv != 0U ? hdiv : 1U);
}

// Starts the global timer of both subsystems in sync, which the LCPU times its sleep with. LPSYS
// must be awake.
static void prv_gtimer_start(void) {
  if ((pbl_readl(HPSYS_AON(ISSR)) & HPSYS_AON_ISSR_LP_ACTIVE) == 0U) {
    return;
  }

  pbl_modifyl(0U, LPSYS_AON_CR_GTIM_EN, PBL_DT_REG_LPSYS_AON + SF32LB57_LPSYS_AON_CR_OFFSET);
  pbl_modifyl(0U, HPSYS_AON_CR_GTIM_EN, HPSYS_AON(CR));
  pbl_writel(1U, HPSYS_AON(GTIMR));
}

void pbl_soc_early_init(void) {
  // Busy waits count cycles, and the boot ROM may have left any clock running
  SystemCoreClock = prv_hclk_hz();

  // HXT48 runs the CPU and the peripherals until the clock tree takes over
  pbl_modifyl(0U, HPSYS_AON_ACR_HXT48_REQ, HPSYS_AON(ACR));
  while ((pbl_readl(HPSYS_AON(ACR)) & HPSYS_AON_ACR_HXT48_RDY) == 0U) {
  }
  if (PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK, pbl_readl(HPSYS_RCC(CSR))) ==
      HPSYS_RCC_CSR_SEL_SYS_HRC48) {
    pbl_modifyl(HPSYS_RCC_CSR_SEL_SYS_MASK,
                PBL_FIELD_PREP(HPSYS_RCC_CSR_SEL_SYS_MASK, HPSYS_RCC_CSR_SEL_SYS_HXT48),
                HPSYS_RCC(CSR));
    SystemCoreClock = prv_hclk_hz();
  }
  pbl_modifyl(0U, HPSYS_RCC_CSR_SEL_PERI, HPSYS_RCC(CSR));

  soc_sf32lb_lpsys_wake();
  prv_gtimer_start();
  soc_sf32lb_lpsys_release();

#ifndef CONFIG_WATCHDOG
  // The boot ROM may leave its watchdog running
  pbl_writel(WDT_CCR_STOP, WDT1(CCR));
  (void)PBL_POLL_BUSY_US((pbl_readl(WDT1(SR)) & WDT_SR_WDT_ACTIVE) == 0U, 1U, 48000U);
#endif

  pbl_modifyl(0U,
              HPSYS_AON_WER_LP2HP_REQ | HPSYS_AON_WER_LP2HP_IRQ | HPSYS_AON_WER_LPTIM1 |
                  HPSYS_AON_WER_GPIO1,
              HPSYS_AON(WER));

  pbl_modifyl(HPSYS_RCC_DWCFGR_SEL_SYS_MASK | HPSYS_RCC_DWCFGR_HDIV_MASK |
                  HPSYS_RCC_DWCFGR_PDIV1_MASK | HPSYS_RCC_DWCFGR_PDIV2_MASK,
              PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_SEL_SYS_MASK, HPSYS_RCC_CSR_SEL_SYS_HRC48) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_HDIV_MASK, DEEPWFI_HDIV) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_PDIV1_MASK, DEEPWFI_PDIV1) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_PDIV2_MASK, DEEPWFI_PDIV2),
              HPSYS_RCC(DWCFGR));
}

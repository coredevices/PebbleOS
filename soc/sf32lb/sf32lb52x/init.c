/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"
#include "syscfg.h"

#include <stdint.h>

#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/drivers/pinctrl/sf32lb52_pins.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_aon.h>
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_hpsys_rcc.h>
#include <hardware/sf32lb52_lpsys_aon.h>
#include <hardware/sf32lb52_pmuc.h>
#include <hardware/sf32lb52_wdt.h>

#define HPSYS_AON(reg) (PBL_DT_REG_HPSYS_AON + SF32LB52_HPSYS_AON_##reg##_OFFSET)
#define HPSYS_RCC(reg) (PBL_DT_REG_HPSYS_RCC + SF32LB52_HPSYS_RCC_##reg##_OFFSET)
#define PMUC(reg)      (PBL_DT_REG_PMUC + SF32LB52_PMUC_##reg##_OFFSET)

#define SYSCLK_HZ   48000000U
#define DLL_STEP_HZ 24000000U

#define PWRKEY_RESET_CNT (32000 * 15)

#define WDT_CMD_TIMEOUT_US 48000U

#define DEEPWFI_HDIV  12U
#define DEEPWFI_PDIV1 0U
#define DEEPWFI_PDIV2 1U

#define PIN_FUNC_MSK \
  (PBL_PINCTRL_SF32LB52_FSEL_MSK | PBL_PINCTRL_SF32LB52_PULL_UP | PBL_PINCTRL_SF32LB52_INPUT)

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
  }

  return sys / (hdiv != 0U ? hdiv : 1U);
}

static void prv_hxt48_on(void) {
  pbl_modifyl(0U, HPSYS_AON_ACR_HXT48_REQ, HPSYS_AON(ACR));
  while ((pbl_readl(HPSYS_AON(ACR)) & HPSYS_AON_ACR_HXT48_RDY) == 0U) {
  }
}

// Starts the global timer of both subsystems in sync, LPSYS must be awake
static void prv_gtimer_start(void) {
  if ((pbl_readl(HPSYS_AON(ISSR)) & HPSYS_AON_ISSR_LP_ACTIVE) == 0U) {
    return;
  }

  pbl_modifyl(0U, LPSYS_AON_CR1_GTIM_EN, PBL_DT_REG_LPSYS_AON + SF32LB52_LPSYS_AON_CR1_OFFSET);
  pbl_modifyl(0U, HPSYS_AON_CR1_GTIM_EN, HPSYS_AON(CR1));
  pbl_writel(1U, HPSYS_AON(GTIMR));
}

#ifdef CONFIG_PBLBOOT
static void prv_wdt_command(uint32_t cmd, bool active) {
  pbl_writel(cmd, PBL_DT_REG_WDT1 + SF32LB52_WDT_CCR_OFFSET);
  (void)PBL_POLL_BUSY_US(
      ((pbl_readl(PBL_DT_REG_WDT1 + SF32LB52_WDT_SR_OFFSET) & WDT_SR_WDT_ACTIVE) != 0U) == active,
      1U, WDT_CMD_TIMEOUT_US);
}

//! PBLBOOT leaves its watchdog running with a timeout of a few seconds, which the watchdog
//! device only takes over once it is up, after slow devices such as a touch controller updating
//! its firmware. Give it our timeout until then, or stop it without the watchdog.
static void prv_wdt_takeover(void) {
  if ((pbl_readl(PBL_DT_REG_WDT1 + SF32LB52_WDT_SR_OFFSET) & WDT_SR_WDT_ACTIVE) == 0U) {
    return;
  }

  prv_wdt_command(WDT_CCR_STOP, false);
#ifdef CONFIG_WATCHDOG
  // At most 32 kHz, the fastest of its clocks
  pbl_writel(32U * CONFIG_WATCHDOG_TIMEOUT_MS, PBL_DT_REG_WDT1 + SF32LB52_WDT_CVR0_OFFSET);
  prv_wdt_command(WDT_CCR_START, true);
#endif
}
#endif

void pbl_soc_early_init(void) {
  // Busy waits count cycles, and the bootloader may have left any clock running
  SystemCoreClock = prv_hclk_hz();

  soc_sf32lb52_boot_flash_delays();

  // Disable default PA21 pull-down, causing leakage
  (void)pbl_pinctrl_sf32lb52_modify(SF32LB52_PA21_GPIO | PBL_PINCTRL_SF32LB52_INPUT, PIN_FUNC_MSK);

  // HXT48 runs the DLLs, and the CPU until HRC48 is calibrated
  prv_hxt48_on();
  if (PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK, pbl_readl(HPSYS_RCC(CSR))) ==
      HPSYS_RCC_CSR_SEL_SYS_HRC48) {
    pbl_modifyl(HPSYS_RCC_CSR_SEL_SYS_MASK,
                PBL_FIELD_PREP(HPSYS_RCC_CSR_SEL_SYS_MASK, HPSYS_RCC_CSR_SEL_SYS_HXT48),
                HPSYS_RCC(CSR));
    SystemCoreClock = prv_hclk_hz();
  }

  pbl_modifyl(0U, HPSYS_RCC_CSR_SEL_PERI, HPSYS_RCC(CSR));

  // Halt LCPU first to avoid LCPU in running state
  soc_sf32lb52_lpsys_wake();
  soc_sf32lb52_lpsys_halt(true);

  soc_sf32lb52_syscfg_init();

  prv_gtimer_start();

#ifdef CONFIG_PBLBOOT
  prv_wdt_takeover();
#endif

  soc_sf32lb52_lpsys_release();

  soc_sf32lb52_pmu_init();

  pbl_modifyl(HPSYS_CFG_SCR_FKEY_MODE, 0U, PBL_DT_REG_HPSYS_CFG + SF32LB52_HPSYS_CFG_SCR_OFFSET);

  // Long power key presses always take the PMIC cold reboot
  pbl_writel(PWRKEY_RESET_CNT, PMUC(PWRKEY_CNT));

  pbl_modifyl(0U,
              HPSYS_AON_WER_LP2HP_REQ | HPSYS_AON_WER_LP2HP_IRQ | HPSYS_AON_WER_LPTIM1 |
                  HPSYS_AON_WER_GPIO1,
              HPSYS_AON(WER));

  // Deep WFI clocks: HRC48, HCLK = 48 MHz / 12 = 4 MHz, PCLK1 = 4 MHz, PCLK2 = 2 MHz.
  // TODO(SF32LB52): to use deep WFI when audio is ON, HCLK needs to remain
  // at 48MHz (div=1). Also, clock needs to be forced ON during deep WFI
  // (FORCE_HP bit in HPSYS_RCC_DBGR register)
  pbl_modifyl(HPSYS_RCC_DWCFGR_SEL_SYS_LP | HPSYS_RCC_DWCFGR_SEL_SYS_MASK |
                  HPSYS_RCC_DWCFGR_HDIV_MASK | HPSYS_RCC_DWCFGR_PDIV1_MASK |
                  HPSYS_RCC_DWCFGR_PDIV2_MASK,
              PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_SEL_SYS_MASK, HPSYS_RCC_CSR_SEL_SYS_HRC48) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_HDIV_MASK, DEEPWFI_HDIV) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_PDIV1_MASK, DEEPWFI_PDIV1) |
                  PBL_FIELD_PREP(HPSYS_RCC_DWCFGR_PDIV2_MASK, DEEPWFI_PDIV2),
              HPSYS_RCC(DWCFGR));
}

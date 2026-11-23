/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/soc/st/exti.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_rcc.h>

#define RCC(reg)  (PBL_DT_REG_RCC + STM32H7_RCC_##reg##_OFFSET)
#define EXTI(reg) (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)

#ifndef CONFIG_CPU_CORTEX_M4
// HPRE and D1CPRE: 0xxx divides by 1, 1000 to 1111 by 2 to 512, skipping 32
static uint32_t prv_ahb_div(uint32_t pre) {
  if ((pre & RCC_AHB_PRE_DIVIDED) == 0U) {
    return 1U;
  }

  pre &= ~RCC_AHB_PRE_DIVIDED;

  return 1U << (pre + (pre >= 4U ? 2U : 1U));
}
#endif

void pbl_soc_stm32h7_wakeup_enable(uint32_t line) {
  pbl_modifyl(0U, PBL_BIT(line % 32U), PBL_DT_REG_EXTI + STM32H7_EXTI_CORE_IMR_OFFSET(line));
}

void pbl_soc_early_init(void) {
  // Lines left unmasked wake this core too, for the interrupts of the other core: it would never
  // take them, nor stop again, and keep the system out of Stop. Drivers unmask the lines of theirs.
  pbl_writel(0U, EXTI(CORE_IMR1));
  pbl_writel(0U, EXTI(CORE_EMR1));
  pbl_writel(0U, EXTI(CORE_IMR2));
  pbl_writel(0U, EXTI(CORE_EMR2));
  pbl_writel(0U, EXTI(CORE_IMR3));
  pbl_writel(0U, EXTI(CORE_EMR3));

#ifdef CONFIG_CPU_CORTEX_M4
  // Started at reset by its option bit, or by the Cortex-M7, the Cortex-M4 waits for the Cortex-M7
  // to set up the clock tree and start it; its clock driver reads the rate it then runs at
  while ((pbl_readl(RCC(GCR)) & RCC_GCR_BOOT_C2) == 0U) {
  }

  // The flash, in the domain of the Cortex-M7, has to stay up while this core runs from it: so does
  // that domain, until this core stops too
  pbl_modifyl(0U, RCC_AHB3ENR_FLASHEN, RCC(AHB3ENR));
#else
  // HSI runs the CPU and the buses until the clock tree takes over
  pbl_modifyl(0U, RCC_CR_HSION, RCC(CR));
  while ((pbl_readl(RCC(CR)) & RCC_CR_HSIRDY) == 0U) {
  }
  pbl_modifyl(RCC_CFGR_SW_MASK, PBL_FIELD_PREP(RCC_CFGR_SW_MASK, RCC_CFGR_SW_HSI), RCC(CFGR));
  while (PBL_FIELD_GET(RCC_CFGR_SWS_MASK, pbl_readl(RCC(CFGR))) != RCC_CFGR_SW_HSI) {
  }

  // Busy waits count CPU cycles
  pbl_cycles_per_sec_set(
      (STM32H7_HSI_HZ >> PBL_FIELD_GET(RCC_CR_HSIDIV_MASK, pbl_readl(RCC(CR)))) /
      prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_D1CPRE_MASK, pbl_readl(RCC(D1CFGR)))));
#endif
}

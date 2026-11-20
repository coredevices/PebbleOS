/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_rcc.h>

#define RCC(reg) (PBL_DT_REG_RCC + STM32H7_RCC_##reg##_OFFSET)

uint32_t SystemCoreClock = STM32H7_HSI_HZ;

// HPRE and D1CPRE: 0xxx divides by 1, 1000 to 1111 by 2 to 512, skipping 32
static uint32_t prv_ahb_div(uint32_t pre) {
  if ((pre & RCC_AHB_PRE_DIVIDED) == 0U) {
    return 1U;
  }

  pre &= ~RCC_AHB_PRE_DIVIDED;

  return 1U << (pre + (pre >= 4U ? 2U : 1U));
}

void pbl_soc_early_init(void) {
  // HSI runs the CPU and the buses until the clock tree takes over
  pbl_modifyl(0U, RCC_CR_HSION, RCC(CR));
  while ((pbl_readl(RCC(CR)) & RCC_CR_HSIRDY) == 0U) {
  }
  pbl_modifyl(RCC_CFGR_SW_MASK, PBL_FIELD_PREP(RCC_CFGR_SW_MASK, RCC_CFGR_SW_HSI), RCC(CFGR));
  while (PBL_FIELD_GET(RCC_CFGR_SWS_MASK, pbl_readl(RCC(CFGR))) != RCC_CFGR_SW_HSI) {
  }

  // Busy waits count CPU cycles
  SystemCoreClock = (STM32H7_HSI_HZ >> PBL_FIELD_GET(RCC_CR_HSIDIV_MASK, pbl_readl(RCC(CR)))) /
                    prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_D1CPRE_MASK, pbl_readl(RCC(D1CFGR))));
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdint.h>

#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_lpsys_aon.h>
#include <hardware/sf32lb52_lpsys_rcc.h>

#define LPSYS_AON(reg) (SF32LB52_LPSYS_AON_BASE + SF32LB52_LPSYS_AON_##reg##_OFFSET)
#define LPSYS_RSTR1    (SF32LB52_LPSYS_RCC_BASE + SF32LB52_LPSYS_RCC_RSTR1_OFFSET)

void soc_sf32lb52_lpsys_halt(bool all) {
  const uint32_t lines = all ? UINT32_MAX : (LPSYS_RCC_RSTR1_LCPU | LPSYS_RCC_RSTR1_MAC);

  if ((pbl_readl(LPSYS_AON(PMR)) & LPSYS_AON_PMR_CPUWAIT) != 0U) {
    return;
  }

  pbl_modifyl(0U, LPSYS_AON_PMR_CPUWAIT, LPSYS_AON(PMR));

  pbl_writel(lines, LPSYS_RSTR1);
  while (pbl_readl(LPSYS_RSTR1) == 0U) {
  }

  if ((pbl_readl(LPSYS_AON(SLP_CTRL)) & LPSYS_AON_SLP_CTRL_SLEEP_STATUS) != 0U) {
    pbl_modifyl(0U, LPSYS_AON_SLP_CTRL_WKUP_REQ, LPSYS_AON(SLP_CTRL));
    while ((pbl_readl(LPSYS_AON(SLP_CTRL)) & LPSYS_AON_SLP_CTRL_SLEEP_STATUS) != 0U) {
    }
  }

  pbl_modifyl(lines, 0U, LPSYS_RSTR1);
}

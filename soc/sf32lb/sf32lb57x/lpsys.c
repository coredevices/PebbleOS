/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_lpsys_rcc.h>
#include <hardware/sf32lb57_hpsys_aon.h>
#include <hardware/sf32lb57_lpsys_aon.h>

#define ISSR           (PBL_DT_REG_HPSYS_AON + SF32LB57_HPSYS_AON_ISSR_OFFSET)
#define LPSYS_AON(reg) (PBL_DT_REG_LPSYS_AON + SF32LB57_LPSYS_AON_##reg##_OFFSET)
#define LPSYS_RSTR1    (PBL_DT_REG_LPSYS_RCC + SF32LB52_LPSYS_RCC_RSTR1_OFFSET)

// LPSYS sees the request after one LP clock cycle, the delay doubles it
#define WAKE_REQ_DELAY_US 230U
// Time for HPSYS to see LP_ACTIVE updated
#define WAKE_ACK_DELAY_US 30U

#define WAKE_COUNT_MAX 20U

static uint32_t s_wake_count;

static void prv_wait_active(void) {
  while ((pbl_readl(ISSR) & HPSYS_AON_ISSR_LP_ACTIVE) == 0U) {
  }
}

void soc_sf32lb_lpsys_wake(void) {
  pbl_modifyl(0U, HPSYS_AON_ISSR_HP2LP_REQ, ISSR);
  pbl_busy_wait_us(WAKE_REQ_DELAY_US);
  prv_wait_active();
  pbl_busy_wait_us(WAKE_ACK_DELAY_US);
  prv_wait_active();

  pbl_irq_lock();
  PBL_ASSERTN(s_wake_count < WAKE_COUNT_MAX);
  s_wake_count++;
  pbl_irq_unlock();
}

void soc_sf32lb_lpsys_release(void) {
  pbl_irq_lock();
  PBL_ASSERTN(s_wake_count > 0U);
  if (--s_wake_count == 0U) {
    pbl_modifyl(HPSYS_AON_ISSR_HP2LP_REQ, 0U, ISSR);
  }
  pbl_irq_unlock();
}

void soc_sf32lb_lpsys_halt(bool all) {
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

void soc_sf32lb_lpsys_set_entry(uint32_t sp, uint32_t pc) {
  pbl_writel(sp, LPSYS_AON(SPR));
  pbl_writel(pc, LPSYS_AON(PCR));
}

void soc_sf32lb_lpsys_run(void) {
  pbl_modifyl(LPSYS_AON_PMR_CPUWAIT, 0U, LPSYS_AON(PMR));
}

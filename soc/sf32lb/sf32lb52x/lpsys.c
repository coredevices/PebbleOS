/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_aon.h>
#include <hardware/sf32lb52_lpsys_aon.h>

#define ISSR           (PBL_DT_REG_HPSYS_AON + SF32LB52_HPSYS_AON_ISSR_OFFSET)
#define LPSYS_AON(reg) (PBL_DT_REG_LPSYS_AON + SF32LB52_LPSYS_AON_##reg##_OFFSET)

// LPSYS sees the request after one LP clock cycle, the delay doubles it
#define WAKE_REQ_DELAY_US 230U
// Time for HPSYS to see LP_ACTIVE updated
#define WAKE_ACK_DELAY_US 30U

#define WAKE_COUNT_MAX 20U

static uint32_t s_wake_count;
// With no program on its CPU, LPSYS only sleeps when forced to, again after each wake
static bool s_parked;

static void prv_force_sleep(void) {
  pbl_modifyl(0U, LPSYS_AON_ANACR_PB_ISO | LPSYS_AON_ANACR_VLP_ISO, LPSYS_AON(ANACR));
  pbl_writel(PBL_FIELD_PREP(LPSYS_AON_PMR_MODE_MASK, LPSYS_AON_PMR_MODE_STANDBY) |
                 LPSYS_AON_PMR_CPUWAIT | LPSYS_AON_PMR_FORCE_SLEEP,
             LPSYS_AON(PMR));
}

static void prv_wait_active(void) {
  while ((pbl_readl(ISSR) & HPSYS_AON_ISSR_LP_ACTIVE) == 0U) {
  }
}

// A forced sleep holds through a wake request: end it, and wake LPSYS if it is asleep
static void prv_end_forced_sleep(void) {
  pbl_modifyl(LPSYS_AON_PMR_MODE_MASK | LPSYS_AON_PMR_FORCE_SLEEP, 0U, LPSYS_AON(PMR));

  if ((pbl_readl(LPSYS_AON(SLP_CTRL)) & LPSYS_AON_SLP_CTRL_SLEEP_STATUS) != 0U) {
    pbl_modifyl(0U, LPSYS_AON_SLP_CTRL_WKUP_REQ, LPSYS_AON(SLP_CTRL));
    while ((pbl_readl(LPSYS_AON(SLP_CTRL)) & LPSYS_AON_SLP_CTRL_SLEEP_STATUS) != 0U) {
    }
  }
}

void soc_sf32lb_lpsys_wake(void) {
  bool forced;

  pbl_irq_lock();
  PBL_ASSERTN(s_wake_count < WAKE_COUNT_MAX);
  forced = s_parked && s_wake_count == 0U;
  s_wake_count++;
  pbl_modifyl(0U, HPSYS_AON_ISSR_HP2LP_REQ, ISSR);
  pbl_irq_unlock();

  if (forced) {
    prv_end_forced_sleep();
  }

  pbl_busy_wait_us(WAKE_REQ_DELAY_US);
  prv_wait_active();
  pbl_busy_wait_us(WAKE_ACK_DELAY_US);
  prv_wait_active();

  if (forced) {
    pbl_modifyl(LPSYS_AON_ANACR_PB_ISO | LPSYS_AON_ANACR_VLP_ISO, 0U, LPSYS_AON(ANACR));
  }
}

void soc_sf32lb_lpsys_release(void) {
  pbl_irq_lock();
  PBL_ASSERTN(s_wake_count > 0U);
  if (--s_wake_count == 0U) {
    pbl_modifyl(HPSYS_AON_ISSR_HP2LP_REQ, 0U, ISSR);
    if (s_parked) {
      prv_force_sleep();
    }
  }
  pbl_irq_unlock();
}

void soc_sf32lb_lpsys_sleep(void) {
  soc_sf32lb_lpsys_halt(false);

  pbl_modifyl(0U, LPSYS_AON_WER_HP2LP_REQ, LPSYS_AON(WER));

  pbl_irq_lock();
  s_parked = true;
  pbl_irq_unlock();
}

void soc_sf32lb_lpsys_run(void) {
  pbl_irq_lock();
  s_parked = false;
  pbl_irq_unlock();

  pbl_modifyl(LPSYS_AON_ANACR_PB_ISO | LPSYS_AON_ANACR_VLP_ISO, 0U, LPSYS_AON(ANACR));
  pbl_modifyl(LPSYS_AON_PMR_MODE_MASK | LPSYS_AON_PMR_CPUWAIT | LPSYS_AON_PMR_FORCE_SLEEP, 0U,
              LPSYS_AON(PMR));
}

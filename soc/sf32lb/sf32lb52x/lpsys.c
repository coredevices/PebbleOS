/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_aon.h>

#define ISSR (PBL_DT_REG_HPSYS_AON + SF32LB52_HPSYS_AON_ISSR_OFFSET)

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

void soc_sf32lb52_lpsys_wake(void) {
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

void soc_sf32lb52_lpsys_release(void) {
  pbl_irq_lock();
  PBL_ASSERTN(s_wake_count > 0U);
  if (--s_wake_count == 0U) {
    pbl_modifyl(HPSYS_AON_ISSR_HP2LP_REQ, 0U, ISSR);
  }
  pbl_irq_unlock();
}

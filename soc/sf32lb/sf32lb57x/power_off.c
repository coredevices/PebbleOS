/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <stdint.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb57_pmuc.h>

#define PMUC(reg) (PBL_DT_REG_PMUC + SF32LB57_PMUC_##reg##_OFFSET)

#define PULSE_US 100000U

// Hibernate keeps the RTC running; only the PMU wake pins drivers armed wake it
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();

  soc_sf32lb_lpsys_wake();
  soc_sf32lb_lpsys_halt(true);

  pbl_modifyl(~PMUC_WER_PIN_MASK, 0U, PMUC(WER));
  pbl_writel(pbl_readl(PMUC(WSR)), PMUC(WCR));

  // The PMU acts on the rising edge
  if ((pbl_readl(PMUC(CR)) & PMUC_CR_HIBER_EN) != 0U) {
    pbl_modifyl(PMUC_CR_HIBER_EN, 0U, PMUC(CR));
    pbl_busy_wait_us(PULSE_US);
  }
  pbl_modifyl(0U, PMUC_CR_HIBER_EN, PMUC(CR));

  while (true) {
  }
}

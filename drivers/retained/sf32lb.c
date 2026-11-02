/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_rtc.h>

uint32_t pbl_retained_read(uint32_t reg) {
  PBL_ASSERTN(reg < SF32LB52_RTC_BKPR_NUM);
  return pbl_readl(SF32LB52_RTC_BASE + SF32LB52_RTC_BKPR_OFFSET(reg));
}

void pbl_retained_write(uint32_t reg, uint32_t value) {
  PBL_ASSERTN(reg < SF32LB52_RTC_BKPR_NUM);
  pbl_writel(value, SF32LB52_RTC_BASE + SF32LB52_RTC_BKPR_OFFSET(reg));
}

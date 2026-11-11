/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/mmio.h>
#include <pbl/system/rtc_registers.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_rtc.h>

void retained_write(uint8_t id, uint32_t value) {
  pbl_writel(value, PBL_DT_REG_RTC + SF32LB52_RTC_BKP_OFFSET(id));
}

uint32_t retained_read(uint8_t id) {
  return pbl_readl(PBL_DT_REG_RTC + SF32LB52_RTC_BKP_OFFSET(id));
}

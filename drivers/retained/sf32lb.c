/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_rtc.h>

/* Only backup registers 2 and 5 to 9 are free for the firmware */
static uint32_t prv_index(enum pbl_retained_reg reg) {
  switch (reg) {
    case PBL_RETAINED_BOOTBITS:
      return 2U;
    case PBL_RETAINED_REBOOT_REASON_0:
      return 5U;
    case PBL_RETAINED_REBOOT_REASON_1:
      return 6U;
    case PBL_RETAINED_REBOOT_REASON_2:
      return 7U;
    case PBL_RETAINED_REBOOT_REASON_3:
      return 8U;
    case PBL_RETAINED_LAST_APP_SLOT:
      return 9U;
    default:
      pbl_panic("unmapped retained register %d", reg);
  }
}

static uintptr_t prv_addr(enum pbl_retained_reg reg) {
  return SF32LB52_RTC_BASE + SF32LB52_RTC_BKPR_OFFSET(prv_index(reg));
}

uint32_t pbl_retained_read(enum pbl_retained_reg reg) {
  return pbl_readl(prv_addr(reg));
}

void pbl_retained_write(enum pbl_retained_reg reg, uint32_t value) {
  pbl_writel(value, prv_addr(reg));
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mmio.h>

#include <board/board.h>

#define RTC_BACKUP_OFFSET(n) (0x40 + ((n) * 4))
#define RTC_BACKUP_NUM       16U

uint32_t pbl_retained_read(uint32_t reg) {
  PBL_ASSERTN(reg < RTC_BACKUP_NUM);
  return pbl_readl(QEMU_RTC_BASE + RTC_BACKUP_OFFSET(reg));
}

void pbl_retained_write(uint32_t reg, uint32_t value) {
  PBL_ASSERTN(reg < RTC_BACKUP_NUM);
  pbl_writel(value, QEMU_RTC_BASE + RTC_BACKUP_OFFSET(reg));
}

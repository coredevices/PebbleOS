/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/mmio.h>

#include <board/board.h>

#define RTC_BACKUP_OFFSET(n) (0x40 + ((n) * 4))

/* Backup register 0 holds the emulator settings */
static uint32_t prv_index(enum pbl_retained_reg reg) {
  switch (reg) {
    case PBL_RETAINED_BOOTBITS:
      return 1U;
    case PBL_RETAINED_BOOT_VERSION:
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
      return 10U;
    case PBL_RETAINED_TZ_0:
      return 11U;
    case PBL_RETAINED_TZ_1:
      return 12U;
    case PBL_RETAINED_TZ_2:
      return 13U;
    case PBL_RETAINED_TZ_3:
      return 14U;
    case PBL_RETAINED_TZ_4:
      return 15U;
    default:
      pbl_panic("unmapped retained register %d", reg);
  }
}

static uintptr_t prv_addr(enum pbl_retained_reg reg) {
  return QEMU_RTC_BASE + RTC_BACKUP_OFFSET(prv_index(reg));
}

uint32_t pbl_retained_read(enum pbl_retained_reg reg) {
  return pbl_readl(prv_addr(reg));
}

void pbl_retained_write(enum pbl_retained_reg reg, uint32_t value) {
  pbl_writel(value, prv_addr(reg));
}

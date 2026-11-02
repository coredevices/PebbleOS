/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/crc/crc.h>
#include <pbl/drivers/retained.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>

#define NUM_WORDS 64U
#define CRC_WORD  31U

static uint32_t PBL_SECTION(".retained") s_retained[NUM_WORDS];

static uint32_t prv_index(enum pbl_retained_reg reg) {
  switch (reg) {
    case PBL_RETAINED_BOOTBITS:
      return 0U;
    case PBL_RETAINED_BOOT_VERSION:
      return 2U;
    case PBL_RETAINED_RTC_TIME:
      return 3U;
    case PBL_RETAINED_RTC_TICKS:
      return 4U;
    case PBL_RETAINED_REBOOT_REASON_0:
      return 5U;
    case PBL_RETAINED_REBOOT_REASON_1:
      return 6U;
    case PBL_RETAINED_REBOOT_REASON_2:
      return 7U;
    case PBL_RETAINED_REBOOT_REASON_3:
      return 8U;
    case PBL_RETAINED_TZ_0:
      return 12U;
    case PBL_RETAINED_TZ_1:
      return 13U;
    case PBL_RETAINED_TZ_2:
      return 14U;
    case PBL_RETAINED_TZ_3:
      return 15U;
    case PBL_RETAINED_TZ_4:
      return 16U;
    case PBL_RETAINED_LAST_APP_SLOT:
      return 19U;
    default:
      pbl_panic("unmapped retained register %d", reg);
  }
}

uint32_t pbl_retained_read(enum pbl_retained_reg reg) {
  return s_retained[prv_index(reg)];
}

void pbl_retained_write(enum pbl_retained_reg reg, uint32_t value) {
  s_retained[prv_index(reg)] = value;
  s_retained[CRC_WORD] = pbl_crc32(0, s_retained, CRC_WORD * sizeof(uint32_t));
}

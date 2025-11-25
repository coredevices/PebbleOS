/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash/flash_impl.h"

#include "system/passert.h"
#include "system/rtc_registers.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <stdbool.h>
#include <stdint.h>

// RTC backup registers are reset to zero on a cold boot (power up from a dead
// battery) so the value stored in the backup register corresponding to no erase
// in progress must also be zero. We need to use a nonzero value to store an
// erase to sector zero, so we can't simply store just the address.
//
// We want to store whether an erase is in progress (1 bit), whether the erase
// is for a sector or a subsector (1 bit), and the address being erased (32
// bits) in a single 32-bit RTC register. Since we can't magically compress 34
// bits into 32, we'll need to play some tricks. The address is going to almost
// certainly be less than 32 bits long; we aren't going to be using
// gigabyte-sized flash memories any time soon (at least not with this
// homegrown API), leaving bits free on the high end.

#define ERASE_IN_PROGRESS 0x80000000
#define ERASE_IS_SUBSECTOR 0x40000000
#define ERASE_FLAGS_MASK (ERASE_IN_PROGRESS | ERASE_IS_SUBSECTOR)
#define ERASE_ADDRESS_MASK 0x3FFFFFFF

status_t flash_impl_set_nvram_erase_status(bool is_subsector,
                                           FlashAddress addr) {
  PBL_ASSERTN((addr & ERASE_FLAGS_MASK) == 0); // "Flash address too large to store"
  uint32_t reg = addr | ERASE_IN_PROGRESS;
  if (is_subsector) {
    reg |= ERASE_IS_SUBSECTOR;
  }
  RTC_WriteBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS, reg);
  return S_SUCCESS;
}

status_t flash_impl_clear_nvram_erase_status(void) {
  RTC_WriteBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS, 0);
  return S_SUCCESS;
}

status_t flash_impl_get_nvram_erase_status(bool *is_subsector,
                                           FlashAddress *addr) {
  uint32_t reg = RTC_ReadBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS);
  if (reg == 0) {
    return S_FALSE;
  }

  *addr = reg & ERASE_ADDRESS_MASK;
  *is_subsector = (reg & ERASE_IS_SUBSECTOR) != 0;
  return S_TRUE;
}

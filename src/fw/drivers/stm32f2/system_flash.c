/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/system_flash.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include "system/logging.h"

void system_flash_erase(uint16_t sector) {
  PBL_LOG_VERBOSE("system_flash_erase");

  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

  if (FLASH_EraseSector(sector, VoltageRange_1) != FLASH_COMPLETE) {
    PBL_LOG(LOG_LEVEL_ALWAYS, "failed to erase sector %u", sector);
    return;
  }
}

void system_flash_write_byte(uint32_t address, uint8_t data) {
  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

  if (FLASH_ProgramByte(address, data) != FLASH_COMPLETE) {
    PBL_LOG(LOG_LEVEL_DEBUG, "failed to write address %p", (void*) address);
    return;
  }
}

uint32_t system_flash_read(uint32_t address) {
  uint32_t data = *(volatile uint32_t*) address;
  return data;
}

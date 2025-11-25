/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/bootbits.h"

#include "drivers/rtc.h"
#include "system/logging.h"
#include "system/version.h"
#include "util/crc32.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#define NRF5_COMPATIBLE
#define SF32LB52_COMPATIBLE
#include <mcu.h>

#include <inttypes.h>
#include <stdint.h>

#if MICRO_FAMILY_NRF5

static uint32_t __attribute__((section(".retained"))) retained[256 / 4];

void retained_write(uint8_t id, uint32_t value) {
  retained[id] = value;
  uint32_t crc32_computed = crc32(0, retained, NRF_RETAINED_REGISTER_CRC * 4);
  retained[NRF_RETAINED_REGISTER_CRC] = crc32_computed;
}

uint32_t retained_read(uint8_t id) {
  return retained[id];
}

void boot_bit_init(void) {
  // Make sure that the bootbits have a valid CRC -- otherwise, their
  // in-memory value is probably scrambled and should be reset.
  uint32_t crc32_computed = crc32(0, retained, NRF_RETAINED_REGISTER_CRC * 4);
  if (crc32_computed != retained[NRF_RETAINED_REGISTER_CRC]) {
    PBL_LOG(LOG_LEVEL_WARNING, "Retained register CRC failed: expected CRC %08lx, got CRC %08lx.  Clearing bootbits!", crc32_computed, retained[NRF_RETAINED_REGISTER_CRC]);
    memset(retained, 0, sizeof(retained));
  }

  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    retained_write(RTC_BKP_BOOTBIT_DR, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  current_value |= bit;
  retained_write(RTC_BKP_BOOTBIT_DR, current_value);
}

void boot_bit_clear(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  current_value &= ~bit;
  retained_write(RTC_BKP_BOOTBIT_DR, current_value);
}

bool boot_bit_test(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  return (current_value & bit);
}

void boot_bit_dump(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "0x%"PRIx32, retained_read(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return retained_read(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
}

void boot_version_write(void) {
  if (boot_version_read() == TINTIN_METADATA.version_timestamp) {
    return;
  }
  retained_write(BOOTLOADER_VERSION_REGISTER, TINTIN_METADATA.version_timestamp);
}

uint32_t boot_version_read(void) {
  return retained_read(BOOTLOADER_VERSION_REGISTER);
}

#elif MICRO_FAMILY_SF32LB52

void boot_bit_init(void) {
  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    HAL_Set_backup(RTC_BKP_BOOTBIT_DR, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  uint32_t current_value = HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
  current_value |= bit;
  HAL_Set_backup(RTC_BKP_BOOTBIT_DR, current_value);
}

void boot_bit_clear(BootBitValue bit) {
  uint32_t current_value = HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
  current_value &= ~bit;
  HAL_Set_backup(RTC_BKP_BOOTBIT_DR, current_value);
}

bool boot_bit_test(BootBitValue bit) {
  uint32_t current_value = HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
  return (current_value & bit);
}

void boot_bit_dump(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "0x%"PRIx32, HAL_Get_backup(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
 return HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
}

void boot_version_write(void) {
  if (boot_version_read() == TINTIN_METADATA.version_timestamp) {
    return;
  }
  /* RTC_WriteBackupRegister(BOOTLOADER_VERSION_REGISTER, TINTIN_METADATA.version_timestamp); */
}

uint32_t boot_version_read(void) {
  return 0xABCD1234;
}

#else

void boot_bit_init(void) {
  rtc_init();

  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    RTC_WriteBackupRegister(RTC_BKP_BOOTBIT_DR, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  uint32_t current_value = RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
  current_value |= bit;
  RTC_WriteBackupRegister(RTC_BKP_BOOTBIT_DR, current_value);
}

void boot_bit_clear(BootBitValue bit) {
  uint32_t current_value = RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
  current_value &= ~bit;
  RTC_WriteBackupRegister(RTC_BKP_BOOTBIT_DR, current_value);
}

bool boot_bit_test(BootBitValue bit) {
  uint32_t current_value = RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
  return (current_value & bit);
}

void boot_bit_dump(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "0x%"PRIx32, RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
}

void boot_version_write(void) {
  if (boot_version_read() == TINTIN_METADATA.version_timestamp) {
    return;
  }
  RTC_WriteBackupRegister(BOOTLOADER_VERSION_REGISTER, TINTIN_METADATA.version_timestamp);
}

uint32_t boot_version_read(void) {
  return RTC_ReadBackupRegister(BOOTLOADER_VERSION_REGISTER);
}

#endif

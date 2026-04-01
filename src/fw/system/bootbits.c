/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/bootbits.h"

#include "drivers/flash.h"
#include "drivers/rtc.h"
#include "flash_region/flash_region.h"
#include "system/logging.h"
#include "system/version.h"
#include "util/crc32.h"

#ifdef MICRO_FAMILY_SF32LB52
#include <bf0_hal.h>
#endif

#if MICRO_FAMILY_STM32F4
#include <stm32f4xx.h>
#endif

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
    PBL_LOG_WRN("Retained register CRC failed: expected CRC %08lx, got CRC %08lx.  Clearing bootbits!", crc32_computed, retained[NRF_RETAINED_REGISTER_CRC]);
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
  PBL_LOG_DBG("0x%"PRIx32, retained_read(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return retained_read(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
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
  PBL_LOG_DBG("0x%"PRIx32, HAL_Get_backup(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
 return HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
}

#define PB_VERSION_MAGIC 0x50425652UL

struct pb_version {
  uint32_t magic;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t tweak;
} __attribute__((packed));

_Static_assert(sizeof(struct pb_version) == 8, "pb_version struct must be 8 bytes");

uint32_t boot_version_read(void) {
  struct pb_version version_data;
  uint32_t version;

  flash_read_bytes((uint8_t *)&version_data,
                   FLASH_REGION_BOOTLOADER_END - sizeof(struct pb_version),
                   sizeof(struct pb_version));

  if (version_data.magic != PB_VERSION_MAGIC) {
    return 0UL;
  }

  version = ((uint32_t)version_data.major << 24) |
            ((uint32_t)version_data.minor << 16) |
            ((uint32_t)version_data.patch << 8) |
            (uint32_t)version_data.tweak;

  return version;
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
  PBL_LOG_DBG("0x%"PRIx32, RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
}

void command_boot_bits_get(void) {
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), "bootbits: 0x%"PRIu32, boot_bits_get());
}

uint32_t boot_version_read(void) {
  return RTC_ReadBackupRegister(BOOTLOADER_VERSION_REGISTER);
}

#endif

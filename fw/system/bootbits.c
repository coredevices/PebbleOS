/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/logging/logging.h>
#include <pbl/system/rtc_registers.h>

#include <system/bootbits.h>
#include <system/version.h>

#ifdef CONFIG_SOC_SF32LB52
#include <bf0_hal.h>
#endif

#if MICRO_FAMILY_STM32F4
#include <pbl/kernel/compiler.h>

#include <stm32f4xx.h>
#endif

#if defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX)
// Provided by the QEMU RTC driver
extern void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value);
extern uint32_t RTC_ReadBackupRegister(uint32_t reg_id);
#endif

#include <inttypes.h>
#include <stdint.h>

#ifdef CONFIG_SOC_NRF52
void boot_bit_init(void) {
  // Make sure that the bootbits have a valid CRC -- otherwise, their
  // in-memory value is probably scrambled and should be reset.
  (void)retained_validate();

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
  PBL_LOG_DBG("0x%" PRIx32, retained_read(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return retained_read(RTC_BKP_BOOTBIT_DR);
}

uint32_t boot_version_read(void) {
  return retained_read(BOOTLOADER_VERSION_REGISTER);
}

#elifdef CONFIG_SOC_SF32LB52
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
  PBL_LOG_DBG("0x%" PRIx32, HAL_Get_backup(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return HAL_Get_backup(RTC_BKP_BOOTBIT_DR);
}

#define PB_VERSION_MAGIC 0x50425652UL

struct pb_version {
  uint32_t magic;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t tweak;
} PBL_PACKED;

static_assert(sizeof(struct pb_version) == 8, "pb_version struct must be 8 bytes");

uint32_t boot_version_read(void) {
  struct pb_version version_data;
  uint32_t version;

  pbl_flash_partition_read(PART_BOOTLOADER, PART_BOOTLOADER_SIZE - sizeof(struct pb_version),
                           &version_data, sizeof(version_data));

  if (version_data.magic != PB_VERSION_MAGIC) {
    return 0UL;
  }

  version = ((uint32_t)version_data.major << 24) | ((uint32_t)version_data.minor << 16) |
            ((uint32_t)version_data.patch << 8) | (uint32_t)version_data.tweak;

  return version;
}

#else

void boot_bit_init(void) {
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
  PBL_LOG_DBG("0x%" PRIx32, RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR));
}

uint32_t boot_bits_get(void) {
  return RTC_ReadBackupRegister(RTC_BKP_BOOTBIT_DR);
}

uint32_t boot_version_read(void) {
  return RTC_ReadBackupRegister(BOOTLOADER_VERSION_REGISTER);
}

#endif

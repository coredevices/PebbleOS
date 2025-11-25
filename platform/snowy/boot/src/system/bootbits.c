/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/bootbits.h"

#include "system/logging.h"
#include "system/rtc_registers.h"
#include "util/version.h"

#include "stm32f4xx.h"

#include <inttypes.h>
#include <stdint.h>

void boot_bit_init(void) {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  PWR_BackupAccessCmd(ENABLE);  // Disable write-protect on RTC_BKP_x registers

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

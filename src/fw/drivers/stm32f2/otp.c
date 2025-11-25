/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/otp.h"

#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#if defined(MICRO_FAMILY_STM32F7)
// See page 83 of STM Reference Manual RM0410:
# define OTP_SLOTS_BASE_ADDR (0x1FF0F000)
# define OTP_LOCKS_BASE_ADDR (0x1FF0F400)
# define OTP_SLOT_SIZE_BYTES (64)
#else
// See page 53 of STM Reference Manual RM0033:
# define OTP_SLOTS_BASE_ADDR (0x1FFF7800)
# define OTP_LOCKS_BASE_ADDR (0x1FFF7A00)
# define OTP_SLOT_SIZE_BYTES (32)
#endif

char * otp_get_slot(const uint8_t index) {
  PBL_ASSERTN(index < NUM_OTP_SLOTS);
  return (char * const) (OTP_SLOTS_BASE_ADDR + (OTP_SLOT_SIZE_BYTES * index));
}

uint8_t * otp_get_lock(const uint8_t index) {
  PBL_ASSERTN(index < NUM_OTP_SLOTS);
  return (uint8_t * const) (OTP_LOCKS_BASE_ADDR + index);
}

bool otp_is_locked(const uint8_t index) {
  return (*otp_get_lock(index) == 0);
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  if (otp_is_locked(index)) {
    return OtpWriteFailAlreadyWritten;
  }
  char * const field = otp_get_slot(index);
  uint8_t * const lock = otp_get_lock(index);

  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
  bool failed = false; // Because it's OTP we need to keep trying
                       // and report failure afterwards for the whole operation
  for(size_t i = 0; i < strlen(value) + 1; i++) {
    if (FLASH_ProgramByte((uint32_t)&field[i], value[i]) != FLASH_COMPLETE) {
      failed = true;
    }
  }
  // Lock the OTP sector
  if (FLASH_ProgramByte((uint32_t)lock, 0) != FLASH_COMPLETE) {
    failed = true;
  }

  FLASH_Lock();

  if(failed) {
    return OtpWriteFailCorrupt;
  } else {
    return OtpWriteSuccess;
  }
}

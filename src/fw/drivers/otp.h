/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#if PLATFORM_ASTERIX || PLATFORM_OBELIX || PLATFORM_GETAFIX || PLATFORM_QEMU_EMERY || PLATFORM_QEMU_FLINT || PLATFORM_QEMU_GABBRO
enum {
  OTP_HWVER = 0,
  OTP_SERIAL = 1,
  OTP_PCBA_SERIAL = 2,
  NUM_OTP_SLOTS = 3,
};
#else
#error "OTP Slots not set for platform"
#endif

typedef enum {
  OtpWriteSuccess = 0,
  OtpWriteFailAlreadyWritten = 1,
  OtpWriteFailCorrupt = 2,
} OtpWriteResult;

uint8_t * otp_get_lock(const uint8_t index);
bool otp_is_locked(const uint8_t index);

char * otp_get_slot(const uint8_t index);
OtpWriteResult otp_write_slot(const uint8_t index, const char *value);

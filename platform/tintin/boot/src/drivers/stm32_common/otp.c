/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/otp.h"

#include "stm32f2xx_flash.h"

#include <stdint.h>
#include <stdbool.h>

// See page 53 of STM Reference Manual RM0033:
#define OTP_SLOTS_BASE_ADDR (0x1FFF7800)
#define OTP_LOCKS_BASE_ADDR (0x1FFF7A00)

//! Each OTP slot is 32 bytes. There are 16 slots: [0-15]
char * otp_get_slot(const uint8_t index) {
  return (char * const) (OTP_SLOTS_BASE_ADDR + (32 * index));
}

uint8_t * otp_get_lock(const uint8_t index) {
  return (uint8_t * const) (OTP_LOCKS_BASE_ADDR + index);
}

bool otp_is_locked(const uint8_t index) {
  return (*otp_get_lock(index) == 0);
}

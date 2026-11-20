/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/otp.h>

// No OTP on the host: every slot reads as locked and empty.

static char s_slot[32];

char *otp_get_slot(const uint8_t index) {
  return s_slot;
}

bool otp_is_locked(const uint8_t index) {
  return true;
}

int otp_write_slot(const uint8_t index, const char *value) {
  return -EEXIST;
}

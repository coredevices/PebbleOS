/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/otp.h>

#define FLASH_ERASE_VAL 0xFFU
#define OTP_SLOT_SIZE 32U
#define SEC_REG_IDX 0x0U

static char s_slot[NUM_OTP_SLOTS][OTP_SLOT_SIZE];

static const struct pbl_flash_sec_regs *prv_sec_regs(void) {
  const struct pbl_flash_sec_regs *regs = FLASH->sec_regs;

  return (regs != NULL && regs->count > 0U) ? regs : NULL;
}

char *otp_get_slot(const uint8_t index) {
  const struct pbl_flash_sec_regs *regs = prv_sec_regs();

  if (index >= NUM_OTP_SLOTS || regs == NULL) {
    return NULL;
  }

  for (uint8_t i = 0U; i < OTP_SLOT_SIZE; i++) {
    int ret = pbl_flash_sec_reg_read(FLASH, regs->addrs[SEC_REG_IDX] + index * OTP_SLOT_SIZE + i,
                                     (uint8_t *)&s_slot[index][i]);
    if (ret != 0) {
      return NULL;
    }
  }

  return s_slot[index];
}

uint8_t *otp_get_lock(const uint8_t index) {
  return NULL;
}

bool otp_is_locked(const uint8_t index) {
  const struct pbl_flash_sec_regs *regs = prv_sec_regs();
  bool locked;

  if (regs == NULL) {
    return false;
  }

  if (pbl_flash_sec_reg_is_locked(FLASH, regs->addrs[SEC_REG_IDX], &locked) != 0) {
    return false;
  }

  return locked;
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  const struct pbl_flash_sec_regs *regs = prv_sec_regs();
  char *existing_val;
  size_t len;

  if (index >= NUM_OTP_SLOTS || regs == NULL) {
    return OtpWriteFailCorrupt;
  }

  len = strlen(value);
  if (len >= OTP_SLOT_SIZE) {
    return OtpWriteFailCorrupt;
  }

  existing_val = otp_get_slot(index);
  for (size_t i = 0U; i < OTP_SLOT_SIZE; i++) {
    if ((uint8_t)existing_val[i] != FLASH_ERASE_VAL) {
      return OtpWriteFailAlreadyWritten;
    }
  }

  for (size_t i = 0U; i <= len; i++) {
    int ret = pbl_flash_sec_reg_write(FLASH, regs->addrs[SEC_REG_IDX] + index * OTP_SLOT_SIZE + i,
                                      (uint8_t)value[i]);
    if (ret != 0) {
      return OtpWriteFailCorrupt;
    }
  }

  existing_val = otp_get_slot(index);
  if ((existing_val == NULL) || (memcmp(existing_val, value, len + 1) != 0)) {
    return OtpWriteFailCorrupt;
  }

  return OtpWriteSuccess;
}

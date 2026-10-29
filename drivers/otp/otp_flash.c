/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/otp.h>

#define FLASH_ERASE_VAL 0xFFU
#define OTP_SLOT_SIZE   32U

static const char *const s_cell_names[NUM_OTP_SLOTS] = {
  [OTP_HWVER] = "hw-version",
  [OTP_SERIAL] = "serial-number",
  [OTP_PCBA_SERIAL] = "pcba-serial-number",
};

static char s_slot[NUM_OTP_SLOTS][OTP_SLOT_SIZE];

static const struct pbl_flash_otp_cell *prv_cell(const uint8_t index) {
  const struct pbl_flash_otp_cell *cell;

  if (index >= NUM_OTP_SLOTS) {
    return nullptr;
  }

  cell = pbl_flash_otp_cell_get(FLASH, s_cell_names[index]);
  if (cell == nullptr || cell->len != OTP_SLOT_SIZE) {
    return nullptr;
  }

  return cell;
}

char *otp_get_slot(const uint8_t index) {
  const struct pbl_flash_otp_cell *cell = prv_cell(index);

  if (cell == nullptr || pbl_flash_otp_read(FLASH, cell->offset, s_slot[index], cell->len) != 0) {
    return nullptr;
  }

  return s_slot[index];
}

uint8_t *otp_get_lock(const uint8_t index) {
  return nullptr;
}

bool otp_is_locked(const uint8_t index) {
  const struct pbl_flash_otp_cell *cell = prv_cell(index);
  struct pbl_flash_otp_info info;
  bool locked;

  if (cell == nullptr || pbl_flash_otp_get_info(FLASH, &info) != 0) {
    return false;
  }

  if (pbl_flash_otp_is_locked(FLASH, (uint8_t)(cell->offset / info.region_len), &locked) != 0) {
    return false;
  }

  return locked;
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  const struct pbl_flash_otp_cell *cell = prv_cell(index);
  char *existing_val;
  size_t len;

  if (cell == nullptr) {
    return OtpWriteFailCorrupt;
  }

  len = strlen(value);
  if (len >= cell->len) {
    return OtpWriteFailCorrupt;
  }

  existing_val = otp_get_slot(index);
  if (existing_val == nullptr) {
    return OtpWriteFailCorrupt;
  }

  for (size_t i = 0U; i < cell->len; i++) {
    if ((uint8_t)existing_val[i] != FLASH_ERASE_VAL) {
      return OtpWriteFailAlreadyWritten;
    }
  }

  if (pbl_flash_otp_write(FLASH, cell->offset, value, len + 1U) != 0) {
    return OtpWriteFailCorrupt;
  }

  existing_val = otp_get_slot(index);
  if ((existing_val == nullptr) || (memcmp(existing_val, value, len + 1U) != 0)) {
    return OtpWriteFailCorrupt;
  }

  return OtpWriteSuccess;
}

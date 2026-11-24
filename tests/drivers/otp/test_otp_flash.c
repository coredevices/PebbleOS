/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/otp.h>

#include <clar.h>

#define REGION_LEN 256U
#define CELL_LEN   32U

// Fakes
//////////////////////////////////////////////////////////

static const struct pbl_flash s_flash;
const struct pbl_flash *const FLASH = &s_flash;

// The three cells share region 0
static const struct pbl_flash_otp_cell s_cells[] = {
  {.name = "hw-version", .offset = 0U, .len = CELL_LEN},
  {.name = "serial-number", .offset = CELL_LEN, .len = CELL_LEN},
  {.name = "pcba-serial-number", .offset = 2U * CELL_LEN, .len = CELL_LEN},
};

static uint8_t s_otp[REGION_LEN];
static bool s_locked;

const struct pbl_flash_otp_cell *pbl_flash_otp_cell_get(const struct pbl_flash *flash,
                                                        const char *name) {
  for (size_t i = 0U; i < sizeof(s_cells) / sizeof(s_cells[0]); i++) {
    if (strcmp(s_cells[i].name, name) == 0) {
      return &s_cells[i];
    }
  }

  return nullptr;
}

int pbl_flash_otp_get_info(const struct pbl_flash *flash, struct pbl_flash_otp_info *info) {
  *info = (struct pbl_flash_otp_info){.region_len = REGION_LEN, .n_regions = 1U};
  return 0;
}

int pbl_flash_otp_read(const struct pbl_flash *flash, uint32_t offs, void *buf, size_t len) {
  memcpy(buf, &s_otp[offs], len);
  return 0;
}

int pbl_flash_otp_write(const struct pbl_flash *flash, uint32_t offs, const void *buf, size_t len) {
  if (s_locked) {
    return -EACCES;
  }

  for (size_t i = 0U; i < len; i++) {
    s_otp[offs + i] &= ((const uint8_t *)buf)[i];
  }
  return 0;
}

int pbl_flash_otp_is_locked(const struct pbl_flash *flash, uint8_t region, bool *locked) {
  cl_assert_equal_i(region, 0);
  *locked = s_locked;
  return 0;
}

// Tests
//////////////////////////////////////////////////////////

void test_otp_flash__initialize(void) {
  memset(s_otp, 0xff, sizeof(s_otp));
  s_locked = false;
}

void test_otp_flash__written_cell_of_locked_region_is_locked(void) {
  cl_assert_equal_i(otp_write_slot(OTP_SERIAL, "SERIAL123456"), 0);
  s_locked = true;

  cl_assert(otp_is_locked(OTP_SERIAL));
  cl_assert_equal_s(otp_get_slot(OTP_SERIAL), "SERIAL123456");
}

void test_otp_flash__blank_cell_of_locked_region_is_not_set(void) {
  cl_assert_equal_i(otp_write_slot(OTP_SERIAL, "SERIAL123456"), 0);
  s_locked = true;

  cl_assert(!otp_is_locked(OTP_HWVER));
  cl_assert(!otp_is_locked(OTP_PCBA_SERIAL));
  cl_assert(otp_get_slot(OTP_HWVER) == nullptr);
  cl_assert(otp_get_slot(OTP_PCBA_SERIAL) == nullptr);
}

void test_otp_flash__unterminated_cell_is_not_set(void) {
  memset(&s_otp[CELL_LEN], 'A', CELL_LEN);
  s_locked = true;

  cl_assert(!otp_is_locked(OTP_SERIAL));
  cl_assert(otp_get_slot(OTP_SERIAL) == nullptr);
}

void test_otp_flash__unlocked_region_is_not_locked(void) {
  cl_assert_equal_i(otp_write_slot(OTP_HWVER, "V1"), 0);

  cl_assert(!otp_is_locked(OTP_HWVER));
  cl_assert_equal_s(otp_get_slot(OTP_HWVER), "V1");
}

void test_otp_flash__blank_cell_is_writable(void) {
  cl_assert_equal_i(otp_write_slot(OTP_HWVER, "V1"), 0);

  cl_assert_equal_i(otp_write_slot(OTP_HWVER, "V2"), -EEXIST);
  cl_assert_equal_i(otp_write_slot(OTP_SERIAL, "SERIAL123456"), 0);
}

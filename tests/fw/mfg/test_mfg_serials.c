/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <signal.h>

#include <pbl/drivers/otp.h>

#include <clar.h>
#include <fake_otp.h>
#include <mfg/mfg_serials.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

// Tests
/////////////////////////////////////////////

void test_mfg_serials__initialize(void) {
  fake_otp_reset();
}

void test_mfg_serials__cleanup(void) {
}

void test_mfg_serials__hw_version(void) {
  const char *hw_version;
  uint8_t index;

  // Initially, bunch of XXs:
  hw_version = mfg_get_hw_version();
  cl_assert(strcmp(hw_version, "XXXXXXXX") == 0);

  // Test writing & reading back:
  const char *written_hw_version1 = "ABCDEFG";
  mfg_write_hw_version(written_hw_version1, strlen(written_hw_version1), &index);
  hw_version = mfg_get_hw_version();
  cl_assert(strcmp(written_hw_version1, hw_version) == 0);
}

void test_mfg_serials__serial_number(void) {
  const char *serial;
  uint8_t index;

  // Initially, bunch of XXs:
  serial = mfg_get_serial_number();
  cl_assert_equal_s(serial, "XXXXXXXXXXXX");

  // Test writing & reading back:
  const char *written_serial1 = "ABCDEFGHIJKL";
  mfg_write_serial_number(written_serial1, strlen(written_serial1), &index);
  serial = mfg_get_serial_number();
  cl_assert_equal_s(written_serial1, serial);
}

void test_mfg_serials__pcba_serial_number(void) {
  const char *pcba_serial;
  uint8_t index;

  // Initially, bunch of XXs:
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(pcba_serial, "XXXXXXXXXXXX");

  // Test writing & reading back:
  const char *written_pcba_serial1 = "01234567901";
  mfg_write_pcba_serial_number(written_pcba_serial1, strlen(written_pcba_serial1), &index);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);

  // Reject overly long writes; original preserved.
  const char *written_pcba_serial_long = "abcdefghijkxyz";
  mfg_write_pcba_serial_number(written_pcba_serial_long, strlen(written_pcba_serial_long), &index);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);

  // OTP_PCBA_SERIAL only has one slot, so subsequent valid writes also fail
  // and the original value is preserved.
  const char *written_pcba_serial2 = "abcdefghijkx";
  mfg_write_pcba_serial_number(written_pcba_serial2, strlen(written_pcba_serial2), &index);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);
}

void test_mfg_serials__serial_number_fails(void) {
  const char *sn;
  uint8_t index;
  MfgSerialsResult r;

  // Initially, return bunch of XXs:
  sn = mfg_get_serial_number();
  cl_assert_equal_s(sn, "XXXXXXXXXXXX");

  // String too long:
  const char *long_sn = "ABCDEFGHIJKLM";
  r = mfg_write_serial_number(long_sn, strlen(long_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(index, 0);
  cl_assert_equal_i(r, MfgSerialsResultFailIncorrectLength);
  cl_assert_equal_s(sn, "XXXXXXXXXXXX");

  // String too short:
  const char *short_sn = "ABCDEFGHIJK";
  r = mfg_write_serial_number(short_sn, strlen(short_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(index, 0);
  cl_assert_equal_i(r, MfgSerialsResultFailIncorrectLength);
  cl_assert_equal_s(sn, "XXXXXXXXXXXX");
}

void test_mfg_serials__serial_numbers(void) {
  const char *sn;
  uint8_t index;
  MfgSerialsResult r;

  // Initially, return bunch of XXs:
  sn = mfg_get_serial_number();
  cl_assert_equal_s(sn, "XXXXXXXXXXXX");

  // First time:
  const char *first_sn = "ABCDEFGHIJKL";
  r = mfg_write_serial_number(first_sn, strlen(first_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(index, OTP_SERIAL);
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, first_sn);

  // OTP_SERIAL only has one slot, so subsequent writes fail and the original
  // value is preserved.
  const char *second_sn = "012345678901";
  r = mfg_write_serial_number(second_sn, strlen(second_sn), &index);
  cl_assert_equal_i(r, MfgSerialsResultFailNoMoreSpace);
  sn = mfg_get_serial_number();
  cl_assert_equal_s(sn, first_sn);
}

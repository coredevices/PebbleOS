/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/mfg_serials.h"
#include "console/prompt_commands.h"
#include "drivers/otp.h"

#include "clar.h"

#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_prompt.h"
#include "fake_otp.h"

#include <signal.h>

extern void command_hwver_write(const char*);
extern void command_pcba_serial_write(const char*);

// Tests
/////////////////////////////////////////////

void test_mfg_serials__initialize(void) {
  fake_otp_reset();
}

void test_mfg_serials__cleanup(void) {
}

void test_mfg_serials__hw_version(void) {
  const char* hw_version;

  // Initially, bunch of XXs:
  hw_version = mfg_get_hw_version();
  cl_assert(strcmp(hw_version, "XXXXXXXX") == 0);

  // Test writing & reading back:
  const char* written_hw_version1 = "ABCDEFG";
  command_hwver_write(written_hw_version1);
  hw_version = mfg_get_hw_version();
  cl_assert(strcmp(written_hw_version1, hw_version) == 0);

}

void test_mfg_serials__serial_number_console(void) {
  const char* serial;

  // Initially, bunch of XXs:
  serial = mfg_get_serial_number();
  cl_assert_equal_s(serial, "XXXXXXXXXXXX");

  // Test writing & reading back:
  const char* written_serial1 = "ABCDEFGHIJKL";
  command_serial_write(written_serial1);
  serial = mfg_get_serial_number();
  cl_assert_equal_s(written_serial1, serial);
}

void test_mfg_serials__pcba_serial_number(void) {
  const char* pcba_serial;

  // Initially, bunch of XXs:
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(pcba_serial, "XXXXXXXXXXXX");

  // Test writing & reading back:
  const char* written_pcba_serial1 = "01234567901";
  command_pcba_serial_write(written_pcba_serial1);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);

  // Reject overly long writes; original preserved.
  const char* written_pcba_serial_long = "abcdefghijkxyz";
  command_pcba_serial_write(written_pcba_serial_long);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);

  // OTP_PCBA_SERIAL only has one slot, so subsequent valid writes also fail
  // and the original value is preserved.
  const char* written_pcba_serial2 = "abcdefghijkx";
  command_pcba_serial_write(written_pcba_serial2);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);
}

void test_mfg_serials__serial_number_fails(void) {
  const char * sn;
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
  const char * sn;
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

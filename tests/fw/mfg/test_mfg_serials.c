/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/mfg_serials.h"
#include "console/prompt_commands.h"

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

#if (BOARD_SILK_BB || BOARD_CALCULUS)
  // Write a second time, too long.
  const char* written_hw_version2_long = "abcdefghijkxyz";
  command_hwver_write(written_hw_version2_long);
  hw_version = mfg_get_hw_version();
  cl_assert_equal_s(written_hw_version1, hw_version);

  // Write second time
  const char* written_hw_version2 = "HIJKLMN";
  command_hwver_write(written_hw_version2);
  hw_version = mfg_get_hw_version();
  cl_assert_equal_s(written_hw_version2, hw_version);

  // Write third time
  const char* written_hw_version3 = "OPQRSTU";
  command_hwver_write(written_hw_version3);
  hw_version = mfg_get_hw_version();
  cl_assert_equal_s(written_hw_version3, hw_version);

  // Write fourth time
  const char* written_hw_version4 = "VWXYZ12";
  command_hwver_write(written_hw_version4);
  hw_version = mfg_get_hw_version();
  cl_assert_equal_s(written_hw_version4, hw_version);

  // Write fifth time
  const char* written_hw_version5 = "3456789";
  command_hwver_write(written_hw_version5);
  hw_version = mfg_get_hw_version();
  cl_assert_equal_s(written_hw_version5, hw_version);
#endif // BOARD_SILK || BOARD_CALCULUS
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

  // Write second time, but too long
  const char* written_pcba_serial2_long = "abcdefghijkxyz";
  command_pcba_serial_write(written_pcba_serial2_long);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial1, pcba_serial);

  // Write second time
  const char* written_pcba_serial2 = "abcdefghijkx";
  command_pcba_serial_write(written_pcba_serial2);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial2, pcba_serial);

  // Write third time
  const char* written_pcba_serial3 = "asdfghjklq";
  command_pcba_serial_write(written_pcba_serial3);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial3, pcba_serial);

  // No more space: Reading should return the last successfully written value.
  const char *pcba_serial4 = "XXXXXXXXXXXX";
  command_pcba_serial_write(pcba_serial4);
  pcba_serial = mfg_get_pcba_serial_number();
  cl_assert_equal_s(written_pcba_serial3, pcba_serial);
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
  cl_assert_equal_i(index, 0);
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, first_sn);

  // Second time:
  const char *second_sn = "012345678901";
  r = mfg_write_serial_number(second_sn, strlen(second_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(index, 3); // SERIAL2 lives at index 3
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, second_sn);

  // Third time:
  const char *third_sn = "!@#$%^&*()-=";
  r = mfg_write_serial_number(third_sn, strlen(third_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, third_sn);
  cl_assert_equal_i(index, 4); // SERIAL3 lives at index 4

  // Fourth time:
  const char *fourth_sn = "mnbvcxzlkjhg";
  r = mfg_write_serial_number(fourth_sn, strlen(fourth_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, fourth_sn);
  cl_assert_equal_i(index, 5); // SERIAL4 lives at index 5

  // Fifth time:
  const char *fifth_sn = "7ujn8ikm9olm";
  r = mfg_write_serial_number(fifth_sn, strlen(fifth_sn), &index);
  sn = mfg_get_serial_number();
  cl_assert_equal_i(r, MfgSerialsResultSuccess);
  cl_assert_equal_s(sn, fifth_sn);
  cl_assert_equal_i(index, 6); // SERIAL5 lives at index 6

  // No more space:
  const char *sixth_sn = "XXXXXXXXXXXX";
  r = mfg_write_serial_number(sixth_sn, strlen(sixth_sn), &index);
  cl_assert(r == MfgSerialsResultFailNoMoreSpace);
}

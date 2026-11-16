/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/print.h>

#include <clar.h>

static char s_out[256];
static size_t s_len;
static int s_calls;

static void prv_output(const char *data, size_t len) {
  memcpy(&s_out[s_len], data, len);
  s_len += len;
  s_out[s_len] = '\0';
  s_calls++;
}

void test_print__initialize(void) {
  s_out[0] = '\0';
  s_len = 0;
  s_calls = 0;
  pbl_print_set_output(prv_output);
}

void test_print__cleanup(void) {
  pbl_print_set_output(nullptr);
}

void test_print__formats_into_one_write(void) {
  pbl_print("*** Booting %s %d ***\n", "v1", 2);
  cl_assert_equal_s(s_out, "*** Booting v1 2 ***\n");
  cl_assert_equal_i(s_calls, 1);
}

void test_print__dropped_without_output(void) {
  pbl_print_set_output(nullptr);
  pbl_print("hidden\n");
  cl_assert_equal_i(s_calls, 0);
}

void test_print__empty_text_writes_nothing(void) {
  pbl_print("%s", "");
  cl_assert_equal_i(s_calls, 0);
}

void test_print__cut_at_127_bytes(void) {
  char line[200];

  memset(line, 'x', sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  pbl_print("%s", line);
  cl_assert_equal_i(s_len, 127);
}

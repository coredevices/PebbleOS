/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/pbl_std/pbl_std.h"
#include "applib/pbl_std/locale.h"

#include "clar.h"

// Stubs
//////////////////////////////////////////////////////////
#include "stubs_heap.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_syscall_internal.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"
#include "stubs_app_state.h"
#include "stubs_worker_state.h"

// Overrides
//////////////////////////////////////////////////////////
void sys_get_time_ms(time_t *t, uint16_t *out_ms) {}

time_t sys_time_utc_to_local(time_t t) {
  return t;
}

int localized_strftime(char* s, size_t maxsize, const char* format,
    const struct tm* tim_p, char *locale) { return 0; }

const char *get_timezone_abbr(void) {
  static const char s_timezone_abbr[] = "A";
  return s_timezone_abbr;
}

void sys_copy_timezone_abbr(char* timezone_abbr, time_t time) {
  const char* sys_tz = get_timezone_abbr();
  strncpy(timezone_abbr, sys_tz, TZ_LEN);
}

struct tm *sys_gmtime_r(const time_t *timep, struct tm *result) {
  return gmtime_r(timep, result);
}

struct tm *sys_localtime_r(const time_t *timep, struct tm *result) {
  return localtime_r(timep, result);
}

// Tests
////////////////////////////////////

void test_pbl_std__get_id(void) {
  const int STR_SIZE = 100;
  char str[STR_SIZE];

  // This is the message we should get back if we try and use floating point
  const char* fp_msg = "floating point not supported in snprintf";

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "%", 1);    // Make sure we don't barf if no type
  cl_assert_equal_s(str, "");
  pbl_snprintf(str, STR_SIZE, "%%", 1);
  cl_assert_equal_s(str, "%");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "%f", 1.0);
  cl_assert_equal_s(str, fp_msg);

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "%s%f%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "%s%d%s", "a", 1, "b");
  cl_assert_equal_s(str, "a1b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc%s %0.1f%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc%s %d%s", "a", 42, "b");
  cl_assert_equal_s(str, "abca 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %3g", 1.0);
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %3d", 42);
  cl_assert_equal_s(str, "abc  42");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "%d %0.12G%s", 4, 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "%d %td%s", 4, 42, "b");
  cl_assert_equal_s(str, "4 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "ab%%c % E zz", 1.0);
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "ab%%c % d zz", 42);
  cl_assert_equal_s(str, "ab%c  42 zz");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %-5e%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %-5d%s", 42, "b");
  cl_assert_equal_s(str, "abc 42   b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %+f%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %+d%s", 42, "b");
  cl_assert_equal_s(str, "abc +42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %lf%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %ld%s", 42, "b");
  cl_assert_equal_s(str, "abc 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %Lf%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %Ld%s", 42, "b");
  cl_assert_equal_s(str, "abc 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %hf%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %hd%s", 42, "b");
  cl_assert_equal_s(str, "abc 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %a%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %jd%s", 42, "b");
  cl_assert_equal_s(str, "abc 42b");

  //----------------------------------------------------
  pbl_snprintf(str, STR_SIZE, "abc %A%s", "a", 1.0, "b");
  cl_assert_equal_s(str, fp_msg);
  pbl_snprintf(str, STR_SIZE, "abc %zd%s", 42, "b");
  cl_assert_equal_s(str, "abc 42b");
}

void test_pbl_std__verify_memcpy_handles_bogus_parameters(void) {
  // See PBL-7873
  uint8_t from = 1;
  uint8_t to;

  // Make sure a normal copy works
  pbl_memcpy(&to, &from, sizeof(from));
  cl_assert_equal_i(to, 1);

  // Make sure a copy with a negative size is a no-op.
  to = 0;
  pbl_memcpy(&to, &from, -sizeof(from));
  cl_assert_equal_i(to, 0);
}

void test_pbl_std__verify_difftime_double_conversion(void) {
  // Can only test positive diffs because of 64 bit vs 32 bit time_t
  cl_assert_equal_i(pbl_override_difftime(30, 10), 20);
  cl_assert_equal_i(pbl_override_difftime(22222222, 1), 22222222 - 1);
  cl_assert_equal_i(pbl_override_difftime(0, 0), 0);
  cl_assert_equal_i(pbl_override_difftime(1, 0), 1);
  cl_assert_equal_i(pbl_override_difftime(2147483647, 0), 2147483647);
}

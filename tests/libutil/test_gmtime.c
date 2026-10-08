/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>
#include <time.h>

#include <pbl/util/time.h>

#include <clar.h>

void test_gmtime__epoch(void) {
  const time_t t = 0;
  struct pbl_tm tm;
  memset(&tm, 0xff, sizeof(tm));

  cl_assert(pbl_gmtime_r(&t, &tm) == &tm);
  cl_assert_equal_i(tm.tm_year, 70);
  cl_assert_equal_i(tm.tm_mon, 0);
  cl_assert_equal_i(tm.tm_mday, 1);
  cl_assert_equal_i(tm.tm_hour, 0);
  cl_assert_equal_i(tm.tm_wday, 4);
  cl_assert_equal_i(tm.tm_isdst, 0);
  cl_assert_equal_i(tm.tm_gmtoff, 0);
  cl_assert_equal_s(tm.tm_zone, "UTC");
}

void test_gmtime__mktime_round_trip(void) {
  for (time_t t = 0; t < INT32_MAX - 12345677; t += 12345677) {
    struct pbl_tm tm;
    pbl_gmtime_r(&t, &tm);
    cl_assert_equal_i(pbl_mktime(&tm), t);
  }
}

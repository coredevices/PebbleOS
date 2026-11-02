/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <limits.h>
#include <math.h>
#include <time.h>

#include <pbl/util/time.h>

#include <clar.h>

// Tests
///////////////////////////////////////////////////////////
void test_mktime__birthdays(void) {
  struct pbl_tm francois_birthday = {
    .tm_sec = 0,
    .tm_min = 44,
    .tm_hour = 10,
    .tm_mday = 30,
    .tm_mon = 4,
    .tm_year = 89,
  };
  cl_assert_equal_i(pbl_mktime(&francois_birthday), 612528240);

  struct pbl_tm rons_birthday = {
    .tm_sec = 17,
    .tm_min = 1,
    .tm_hour = 9,
    .tm_mday = 10,
    .tm_mon = 4,
    .tm_year = 63,
  };
  cl_assert_equal_i(pbl_mktime(&rons_birthday), -1);

  struct pbl_tm alex_marianetti_birthday = {
    .tm_sec = 29,
    .tm_min = 4,
    .tm_hour = 17,
    .tm_mday = 2,
    .tm_mon = 9,
    .tm_year = 107,
  };
  cl_assert_equal_i(pbl_mktime(&alex_marianetti_birthday), 1191344669);

  struct pbl_tm chris_birthday = {
    .tm_sec = 59,
    .tm_min = 3,
    .tm_hour = 10,
    .tm_mday = 15,
    .tm_mon = 5,
    .tm_year = 89,
  };
  cl_assert_equal_i(pbl_mktime(&chris_birthday), 613908239);
}

void test_mktime__epoch(void) {
  struct pbl_tm epoch = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 0,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 70,
  };
  cl_assert_equal_i(pbl_mktime(&epoch), 0);
}

void test_mktime__leap(void) {
  struct pbl_tm real_leap = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 10,
    .tm_mday = 29,
    .tm_mon = 1,
    .tm_year = 112,
  };
  cl_assert_equal_i(pbl_mktime(&real_leap), 1330509600);
}

void test_mktime__after_2038(void) {
  struct pbl_tm tm = {
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 150,
  };
  if (sizeof(time_t) > 4) {
    // 2050-01-01T00:00:00Z
    cl_assert(pbl_mktime(&tm) == (time_t)2524608000LL);
    cl_assert_equal_i(tm.tm_year, 150);
    cl_assert_equal_i(tm.tm_wday, PBL_SATURDAY);
  } else {
    cl_assert(pbl_mktime(&tm) == -1);
  }
}

void test_mktime__gregorian_centuries(void) {
  if (sizeof(time_t) <= 4) {
    struct pbl_tm too_late = {.tm_mday = 1, .tm_year = 201};
    cl_assert(pbl_mktime(&too_late) == -1);
    return;
  }
  // 2099-12-31T23:59:59Z
  struct pbl_tm last_2099 =
      {.tm_sec = 59, .tm_min = 59, .tm_hour = 23, .tm_mday = 31, .tm_mon = 11, .tm_year = 199};
  cl_assert(pbl_mktime(&last_2099) == (time_t)4102444799LL);
  // 2100 is not a leap year: Feb 28 is followed by Mar 1
  struct pbl_tm mar1_2100 = {.tm_mday = 1, .tm_mon = 2, .tm_year = 200};
  cl_assert(pbl_mktime(&mar1_2100) == (time_t)4107542400LL);
  struct pbl_tm feb29_2100 = {.tm_mday = 29, .tm_mon = 1, .tm_year = 200};
  cl_assert(pbl_mktime(&feb29_2100) == (time_t)4107542400LL);
  cl_assert_equal_i(feb29_2100.tm_mon, 2);
  cl_assert_equal_i(feb29_2100.tm_mday, 1);
  // 2106-02-07T06:28:16Z, past unsigned 32 bits
  struct pbl_tm wrap =
      {.tm_sec = 16, .tm_min = 28, .tm_hour = 6, .tm_mday = 7, .tm_mon = 1, .tm_year = 206};
  cl_assert(pbl_mktime(&wrap) == (time_t)4294967296LL);
  // 2200-01-01T00:00:00Z
  struct pbl_tm y2200 = {.tm_mday = 1, .tm_year = 300};
  cl_assert(pbl_mktime(&y2200) == (time_t)7258118400LL);
  cl_assert_equal_i(y2200.tm_wday, PBL_WEDNESDAY);
  // 2400 is a leap year
  struct pbl_tm feb29_2400 = {.tm_mday = 29, .tm_mon = 1, .tm_year = 500};
  cl_assert(pbl_mktime(&feb29_2400) == (time_t)13574563200LL);
  cl_assert_equal_i(feb29_2400.tm_mon, 1);
  // 9999-12-31T23:59:59Z
  struct pbl_tm last =
      {.tm_sec = 59, .tm_min = 59, .tm_hour = 23, .tm_mday = 31, .tm_mon = 11, .tm_year = 8099};
  cl_assert(pbl_mktime(&last) == (time_t)253402300799LL);
  struct pbl_tm too_late = {.tm_mday = 1, .tm_year = 8101};
  cl_assert(pbl_mktime(&too_late) == -1);

  // Fields past their range can't reach outside the supported years either
  struct pbl_tm huge_mday = {.tm_mday = INT_MAX, .tm_year = 126};
  cl_assert(pbl_mktime(&huge_mday) == -1);
  struct pbl_tm negative_mday = {.tm_mday = INT_MIN + 1, .tm_year = 126};
  cl_assert(pbl_mktime(&negative_mday) == -1);
  struct pbl_tm late_mday = {.tm_mday = 365, .tm_mon = 11, .tm_year = 8099};
  cl_assert(pbl_mktime(&late_mday) == -1);

  // Round trip through gmtime every day of a few centuries
  for (time_t t = 4000000000LL; t < 14000000000LL; t += 86400 * 7 + 3601) {
    struct pbl_tm tm;
    pbl_gmtime_r(&t, &tm);
    cl_assert(pbl_mktime(&tm) == t);
  }
}

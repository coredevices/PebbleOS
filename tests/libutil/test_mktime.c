/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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

void test_mktime__before_2100(void) {
  struct pbl_tm last =
      {.tm_sec = 59, .tm_min = 59, .tm_hour = 23, .tm_mday = 31, .tm_mon = 11, .tm_year = 199};
  struct pbl_tm too_late = {.tm_mday = 1, .tm_year = 201};
  if (sizeof(time_t) > 4) {
    // 2099-12-31T23:59:59Z
    cl_assert(pbl_mktime(&last) == (time_t)4102444799LL);
  }
  cl_assert(pbl_mktime(&too_late) == -1);
}

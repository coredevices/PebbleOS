/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/time.h>
#include <pbl/util/units.h>

#include <clar.h>

void test_time__split_seconds(void) {
  uint32_t day, hour, minute, second;

  pbl_time_split_seconds(1, &day, &hour, &minute, &second);
  cl_assert_equal_i(day, 0);
  cl_assert_equal_i(hour, 0);
  cl_assert_equal_i(minute, 0);
  cl_assert_equal_i(second, 1);

  pbl_time_split_seconds(61, &day, &hour, &minute, &second);
  cl_assert_equal_i(day, 0);
  cl_assert_equal_i(hour, 0);
  cl_assert_equal_i(minute, 1);
  cl_assert_equal_i(second, 1);

  pbl_time_split_seconds(3 * PBL_SEC_PER_DAY, &day, &hour, &minute, &second);
  cl_assert_equal_i(day, 3);
  cl_assert_equal_i(hour, 0);
  cl_assert_equal_i(minute, 0);
  cl_assert_equal_i(second, 0);

  pbl_time_split_seconds((3 * PBL_SEC_PER_DAY) + (2 * PBL_SEC_PER_HOUR) + (4 * PBL_SEC_PER_MIN) + 5,
                         &day, &hour, &minute, &second);
  cl_assert_equal_i(day, 3);
  cl_assert_equal_i(hour, 2);
  cl_assert_equal_i(minute, 4);
  cl_assert_equal_i(second, 5);
}

void test_time__leap_year(void) {
  cl_assert(pbl_time_is_leap_year(2024));
  cl_assert(pbl_time_is_leap_year(2000));
  cl_assert(!pbl_time_is_leap_year(1900));
  cl_assert(!pbl_time_is_leap_year(2100));
  cl_assert(!pbl_time_is_leap_year(2026));
}

void test_time__days_in_month(void) {
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int month = 1; month <= 12; month++) {
    cl_assert_equal_i(pbl_time_days_in_month(month, false), days[month - 1]);
    cl_assert_equal_i(pbl_time_days_in_month(month, true), days[month - 1] + (month == 2));
  }
}

void test_time__display_hour(void) {
  cl_assert_equal_i(pbl_time_display_hour(0, true), 0);
  cl_assert_equal_i(pbl_time_display_hour(13, true), 13);
  cl_assert_equal_i(pbl_time_display_hour(0, false), 12);
  cl_assert_equal_i(pbl_time_display_hour(1, false), 1);
  cl_assert_equal_i(pbl_time_display_hour(12, false), 12);
  cl_assert_equal_i(pbl_time_display_hour(23, false), 11);
}

void test_time__minute_of_day_adjust(void) {
  cl_assert_equal_i(pbl_time_minute_of_day_adjust(10, 5), 15);
  cl_assert_equal_i(pbl_time_minute_of_day_adjust(10, -20), PBL_MIN_PER_DAY - 10);
  cl_assert_equal_i(pbl_time_minute_of_day_adjust(PBL_MIN_PER_DAY - 1, 2), 1);
}

void test_time__weekday(void) {
  cl_assert(pbl_time_is_weekday(PBL_MONDAY));
  cl_assert(pbl_time_is_weekday(PBL_FRIDAY));
  cl_assert(!pbl_time_is_weekday(PBL_SATURDAY));
  cl_assert(pbl_time_is_weekend(PBL_SUNDAY));
  cl_assert(pbl_time_is_weekend(PBL_SATURDAY));
  cl_assert(!pbl_time_is_weekend(PBL_WEDNESDAY));
}

static void prv_assert_breakdown(time_t t, int year, int mon, int mday, int hour, int min, int sec,
                                 int wday, int yday) {
  struct tm tm = {0};
  pbl_time_breakdown(t, &tm);
  cl_assert_equal_i(tm.tm_year, year - PBL_TM_YEAR_ORIGIN);
  cl_assert_equal_i(tm.tm_mon, mon);
  cl_assert_equal_i(tm.tm_mday, mday);
  cl_assert_equal_i(tm.tm_hour, hour);
  cl_assert_equal_i(tm.tm_min, min);
  cl_assert_equal_i(tm.tm_sec, sec);
  cl_assert_equal_i(tm.tm_wday, wday);
  cl_assert_equal_i(tm.tm_yday, yday);
}

void test_time__breakdown(void) {
  prv_assert_breakdown(0, 1970, 0, 1, 0, 0, 0, PBL_THURSDAY, 0);
  prv_assert_breakdown(951782400, 2000, 1, 29, 0, 0, 0, PBL_TUESDAY, 59);
  prv_assert_breakdown(1735689599, 2024, 11, 31, 23, 59, 59, PBL_TUESDAY, 365);
  prv_assert_breakdown(1790870400, 2026, 9, 1, 16, 0, 0, PBL_THURSDAY, 273);
  prv_assert_breakdown(-1, 1969, 11, 31, 23, 59, 59, PBL_WEDNESDAY, 364);
}

void test_time__breakdown_keeps_zone_fields(void) {
  struct tm tm = {.tm_isdst = 1, .tm_gmtoff = 3600, .tm_zone = "CET"};
  pbl_time_breakdown(0, &tm);
  cl_assert_equal_i(tm.tm_isdst, 1);
  cl_assert_equal_i(tm.tm_gmtoff, 3600);
  cl_assert_equal_s(tm.tm_zone, "CET");
}

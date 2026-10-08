/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <pbl/services/time.h>
#include <pbl/util/units.h>

#include <fake_rtc.h>

#include <stubs_logging.h>
#include <stubs_passert.h>

#include <string.h>

// 2026-07-01 12:00:00 UTC
#define SUMMER_UTC ((time_t)1782907200)
// 2026-01-15 12:00:00 UTC
#define WINTER_UTC ((time_t)1768478400)
// 2026-03-08 10:00:00 UTC / 2026-11-01 09:00:00 UTC (US DST transitions for -8h)
#define DST_START ((time_t)1772964000)
#define DST_END   ((time_t)1793523600)

static void prv_set_pacific(void) {
  const TimezoneInfo tz = {
    .tm_zone = "P*T",
    .dst_id = 1,
    .timezone_id = 1,
    .tm_gmtoff = -8 * PBL_SEC_PER_HOUR,
    .dst_start = DST_START,
    .dst_end = DST_END,
  };
  time_util_update_timezone(&tz);
}

void test_clock_time__initialize(void) {
  fake_rtc_init(0, SUMMER_UTC);
  prv_set_pacific();
}

void test_clock_time__isdst(void) {
  cl_assert(time_get_isdst(SUMMER_UTC));
  cl_assert(!time_get_isdst(WINTER_UTC));
  cl_assert(time_get_isdst(DST_START));
  cl_assert(!time_get_isdst(DST_START - 1));
  cl_assert(!time_get_isdst(DST_END));
}

void test_clock_time__utc_to_local(void) {
  cl_assert_equal_i(time_utc_to_local(SUMMER_UTC), SUMMER_UTC - 7 * PBL_SEC_PER_HOUR);
  cl_assert_equal_i(time_utc_to_local(WINTER_UTC), WINTER_UTC - 8 * PBL_SEC_PER_HOUR);
  cl_assert_equal_i(time_local_to_utc(time_utc_to_local(SUMMER_UTC)), SUMMER_UTC);
  cl_assert_equal_i(time_local_to_utc(time_utc_to_local(WINTER_UTC)), WINTER_UTC);
}

void test_clock_time__timezone_abbr(void) {
  char abbr[TZ_LEN];
  time_get_timezone_abbr(abbr, SUMMER_UTC);
  cl_assert_equal_s(abbr, "PDT");
  time_get_timezone_abbr(abbr, WINTER_UTC);
  cl_assert_equal_s(abbr, "PST");
}

void test_clock_time__uk_abbr(void) {
  const TimezoneInfo tz = {
    .tm_zone = "B*T",
    .dst_start = DST_START,
    .dst_end = DST_END,
  };
  time_util_update_timezone(&tz);

  char abbr[TZ_LEN];
  time_get_timezone_abbr(abbr, SUMMER_UTC);
  cl_assert_equal_s(abbr, "BST");
  time_get_timezone_abbr(abbr, WINTER_UTC);
  cl_assert_equal_s(abbr, "GMT");
}

void test_clock_time__lord_howe_half_hour_dst(void) {
  const TimezoneInfo tz = {
    .tm_zone = "LHST",
    .dst_id = DSTID_LORDHOWE,
    .tm_gmtoff = 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN,
    .dst_start = DST_START,
    .dst_end = DST_END,
  };
  time_util_update_timezone(&tz);
  cl_assert_equal_i(time_get_dstoffset(), PBL_SEC_PER_HOUR / 2);
  cl_assert_equal_i(time_utc_to_local(SUMMER_UTC), SUMMER_UTC + 11 * PBL_SEC_PER_HOUR);
}

void test_clock_time__localtime(void) {
  const time_t t = SUMMER_UTC;
  struct tm tm;
  cl_assert(localtime_r(&t, &tm) == &tm);
  cl_assert_equal_i(tm.tm_year, 126);
  cl_assert_equal_i(tm.tm_mon, 6);
  cl_assert_equal_i(tm.tm_mday, 1);
  cl_assert_equal_i(tm.tm_hour, 5);
  cl_assert_equal_i(tm.tm_isdst, 1);
  cl_assert_equal_i(tm.tm_gmtoff, -7 * PBL_SEC_PER_HOUR);
  cl_assert_equal_s(tm.tm_zone, "PDT");
}

void test_clock_time__day_helpers(void) {
  cl_assert_equal_i(time_util_get_minute_of_day(SUMMER_UTC), 5 * PBL_MIN_PER_HOUR);
  cl_assert_equal_i(time_util_get_day_in_week(SUMMER_UTC), PBL_WEDNESDAY);
  cl_assert_equal_i(time_util_get_midnight_of(SUMMER_UTC), SUMMER_UTC - 5 * PBL_SEC_PER_HOUR);
  cl_assert_equal_i(time_start_of_today(), SUMMER_UTC - 5 * PBL_SEC_PER_HOUR);
  cl_assert(time_util_range_spans_day(SUMMER_UTC - PBL_SEC_PER_HOUR, SUMMER_UTC + PBL_SEC_PER_DAY,
                                      SUMMER_UTC));
  cl_assert(!time_util_range_spans_day(SUMMER_UTC, SUMMER_UTC + PBL_SEC_PER_HOUR, SUMMER_UTC));
}

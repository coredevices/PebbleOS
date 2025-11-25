/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/cron.h"
#include "util/size.h"

#include <pebbleos/cron.h>

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_regular_timer.h"
#include "fake_rtc.h"

// Tests
///////////////////////////////////////////////////////////
// Thursday 2015 Nov 12, 00:00:00 GMT
static const time_t s_2015_nov12_000000_gmt = 1447286400;
// Thursday 2015 Nov 12, 12:34:56 GMT
static const time_t s_2015_nov12_123456_gmt = 1447331696;
// Saturday 2015 Dec 19, 12:34:56 GMT
static const time_t s_2015_dec19_123456_gmt = 1450528496;

// DST points
static const time_t s_2015_nov20_020000_gmt = 1447984800;
static const time_t s_2015_dec20_020000_gmt = 1450576800;
static const TimezoneInfo s_timezone_gmt = {
  .tm_zone = "GMT",
  .dst_id = 0,
  .timezone_id = 0,
  .tm_gmtoff = 0,
  .dst_start = 0,
  .dst_end = 0,
};

void test_cron__initialize(void) {
  cron_service_init();
}

void test_cron__cleanup(void) {
  cron_service_deinit();
}

static TimezoneInfo g_timezone;
static void prv_set_rtc(time_t t, const TimezoneInfo *tz_info) {
  fake_rtc_init(0, t);
  g_timezone = *tz_info;
  time_util_update_timezone(&g_timezone);
}

static void prv_cron_callback(CronJob *job, void* data) {
  job->cb_data = (void*)((uintptr_t)data + 1);
}

static void prv_clock_change(int32_t time_diff, int32_t gmt_diff, bool dst_trans) {
  PebbleSetTimeEvent set_time_info = {
    .utc_time_delta = time_diff,
    .gmt_offset_delta = gmt_diff,
    .dst_changed = dst_trans,
  };
  rtc_set_time(rtc_get_time() + time_diff);
  g_timezone.tm_gmtoff += gmt_diff;
  time_util_update_timezone(&g_timezone);
  cron_service_handle_clock_change(&set_time_info);
}

void test_cron__time_change_basic(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 45,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,

    .may_be_instant = true,

    .clock_change_tolerance = 0,
  };
  CronJob *job = &test_cron;
  time_t base = s_2015_nov12_123456_gmt;
  prv_set_rtc(base, &s_timezone_gmt);
  // 2015 Nov 12, 12:45:00
  int32_t target = 1447332300;

  cron_clear_all_jobs();
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  cron_job_schedule(job);
  cl_assert_equal_i((uintptr_t)job->cb_data, 0);
  cl_assert_equal_i(job->cached_execute_time, target);
  cl_assert_equal_i(cron_service_get_job_count(), 1);

  // Mutate the execute time to see if we actually effect change.
  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = 10;
  prv_clock_change(0, 0, false);
  cl_assert_equal_i(job->cached_execute_time, UINT32_MAX);
  job->cached_execute_time = UINT32_MAX;
  prv_clock_change(0, 0, true);
  cl_assert_equal_i(job->cached_execute_time, target);
  job->cached_execute_time = UINT32_MAX;
  prv_clock_change(0, 1, false);
  target--; // adjust for GMT offset change
  cl_assert_equal_i(job->cached_execute_time, target);
  job->cached_execute_time = UINT32_MAX;
  prv_clock_change(0, 1, true);
  target--; // adjust for GMT offset change
  cl_assert_equal_i(job->cached_execute_time, target);

  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = 0;
  prv_clock_change(0, 0, false);
  cl_assert_equal_i(job->cached_execute_time, target);

  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = 0;
  prv_clock_change(1, 0, false);
  cl_assert_equal_i(job->cached_execute_time, target);

  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = 1;
  prv_clock_change(0, 0, false);
  cl_assert_equal_i(job->cached_execute_time, UINT32_MAX);

  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = 1;
  prv_clock_change(1, 0, false);
  cl_assert_equal_i(job->cached_execute_time, target);

  job->cached_execute_time = UINT32_MAX;
  job->clock_change_tolerance = UINT32_MAX;
  prv_clock_change(INT32_MAX, 0, false);
  cl_assert_equal_i(job->cached_execute_time, UINT32_MAX);

  cron_clear_all_jobs();
}

void test_cron__time_change_instant(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 35,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,

    .may_be_instant = true,

    .clock_change_tolerance = 0,
  };
  CronJob *job = &test_cron;
  time_t base = s_2015_nov12_123456_gmt;
  prv_set_rtc(base, &s_timezone_gmt);
  // 2015 Nov 12, 12:35:00
  int32_t target = 1447331700;

  cron_clear_all_jobs();
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  cron_job_schedule(job);
  cl_assert_equal_i((uintptr_t)job->cb_data, 0);
  cl_assert_equal_i(job->cached_execute_time, target);
  cl_assert_equal_i(cron_service_get_job_count(), 1);

  // Mutate the execute time to see if we actually effect change.
  job->clock_change_tolerance = 100;
  prv_clock_change(10, 0, false);
  cl_assert_equal_i((uintptr_t)job->cb_data, 1);
  cl_assert_equal_i(job->cached_execute_time, target);
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  cron_clear_all_jobs();
}

static void prv_basic_test(const TimezoneInfo *tz_info, CronJob *job, time_t base,
                           time_t offset, time_t increment, int dst_type) {
  TimezoneInfo new_tz_info = *tz_info;
  switch (dst_type) {
    case 0:
      break;
    case 1:
      new_tz_info.dst_start = s_2015_nov20_020000_gmt;
      new_tz_info.dst_end = s_2015_dec20_020000_gmt;
      break;
    case 2:
      new_tz_info.dst_start = 1;
      new_tz_info.dst_end = INT32_MAX;
      break;
  }
  prv_set_rtc(base, &new_tz_info);

  cron_clear_all_jobs();
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  job->cb_data = (void*)0;

  cron_job_schedule(job);
  cl_assert_equal_i((uintptr_t)job->cb_data, 0);
  cl_assert_equal_i(job->cached_execute_time, base + offset);
  cl_assert_equal_i(cron_service_get_job_count(), 1);

  // Check that the timer doesn't fire early
  if (offset > 0) {
    fake_rtc_increment_time(increment - 1);
    cron_service_wakeup();
    cl_assert_equal_i((uintptr_t)job->cb_data, 0);
    cl_assert_equal_i(job->cached_execute_time, base + offset);
    cl_assert_equal_i(cron_service_get_job_count(), 1);
    fake_rtc_increment_time(1);
  } else {
    fake_rtc_increment_time(increment);
  }

  cron_service_wakeup();
  cl_assert_equal_i((uintptr_t)job->cb_data, 1);
  cl_assert_equal_i(job->cached_execute_time, base + offset);
  cl_assert_equal_i(cron_service_get_job_count(), 0);
}

void test_cron__1_basic(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = CRON_MINUTE_ANY,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,

    .may_be_instant = true,
  };

  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, 0, 0, 0);
}

void test_cron__4_basic(void) {
  CronJob test_cron[4] = {
    { .cb = prv_cron_callback,
      .cb_data = (void*)0,

      .minute = 45,
      .hour = CRON_HOUR_ANY,
      .mday = CRON_MDAY_ANY,
      .month = CRON_MONTH_ANY,

      .may_be_instant = true,
    },
    { .cb = prv_cron_callback,
      .cb_data = (void*)0,

      .minute = CRON_MINUTE_ANY,
      .hour = 13,
      .mday = CRON_MDAY_ANY,
      .month = CRON_MONTH_ANY,

      .may_be_instant = true,
    },
    { .cb = prv_cron_callback,
      .cb_data = (void*)0,

      .minute = CRON_MINUTE_ANY,
      .hour = CRON_HOUR_ANY,
      .mday = 12,
      .month = CRON_MONTH_ANY,

      .may_be_instant = true,
    },
    { .cb = prv_cron_callback,
      .cb_data = (void*)0,

      .minute = CRON_MINUTE_ANY,
      .hour = CRON_HOUR_ANY,
      .mday = CRON_MDAY_ANY,
      .month = 11,

      .may_be_instant = true,
    },
  };
  int timestamps[4] = {
    1447332300, // 2015 Nov 12, 12:45:00 GMT
    1447333200, // 2015 Nov 12, 13:00:00 GMT
    1447372800, // 2015 Nov 13, 00:00:00 GMT
    1448928000, // 2015 Dec  1, 00:00:00 GMT
  };

  prv_set_rtc(s_2015_nov12_123456_gmt, &s_timezone_gmt);

  cron_clear_all_jobs();
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  // Add the jobs in reverse order to make sure they add properly.
  for (int i = 0; i < 4; i++) {
    CronJob *job = &test_cron[4 - i - 1];
    cron_job_schedule(job);
    cl_assert_equal_i((uintptr_t)job->cb_data, 0);
    cl_assert_equal_i(job->cached_execute_time, timestamps[4 - i - 1]);
    cl_assert_equal_i(cron_service_get_job_count(), i+1);
  }

  time_t left = s_2015_nov12_123456_gmt;
  for (int i = 0; i < 4; i++) {
    fake_rtc_increment_time(timestamps[i] - left);
    left = timestamps[i];
    cron_service_wakeup();
    cl_assert_equal_i(cron_service_get_job_count(), 4 - i - 1);
    for (int l = 0; l < 4; l++) {
      cl_assert_equal_i((uintptr_t)test_cron[l].cb_data, i >= l ? 1 : 0);
    }
  }
}

void test_cron__already_elapsed(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = CRON_MINUTE_ANY,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,

    .may_be_instant = true,
  };

  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, 0, SECONDS_PER_MINUTE, 0);
}

struct {
  int month, mday, hour, minute;
  int8_t wday;
  time_t dest_time;
} s_cron_test_info[] = {
  //////// 'future' time finding
  // minute
  // 2015 Nov 12, 12:45:00
  { -1,-1,-1,45, WDAY_ANY, 1447332300},
  // hour
  // 2015 Nov 12, 13:00:00
  { -1,-1,13,-1, WDAY_ANY, 1447333200},
  // hour+minute
  // 2015 Nov 12, 13:45:00
  { -1,-1,13,45, WDAY_ANY, 1447335900},
  // mday
  // 2015 Nov 13, 00:00:00
  { -1,12,-1,-1, WDAY_ANY, 1447372800},
  // mday+minute
  // 2015 Nov 13, 00:45:00
  { -1,12,-1,45, WDAY_ANY, 1447375500},
  // mday+hour
  // 2015 Nov 13, 13:00:00
  { -1,12,13,-1, WDAY_ANY, 1447419600},
  // mday+hour+minute
  // 2015 Nov 13, 13:45:00
  { -1,12,13,45, WDAY_ANY, 1447422300},
  // month
  // 2015 Dec  1, 00:00:00
  { 11,-1,-1,-1, WDAY_ANY, 1448928000},
  // month+minute
  // 2015 Dec  1, 00:45:00
  { 11,-1,-1,45, WDAY_ANY, 1448930700},
  // month+hour
  // 2015 Dec  1, 13:00:00
  { 11,-1,13,-1, WDAY_ANY, 1448974800},
  // month+hour+minute
  // 2015 Dec  1, 13:45:00
  { 11,-1,13,45, WDAY_ANY, 1448977500},
  // month+mday
  // 2015 Dec 13, 00:00:00
  { 11,12,-1,-1, WDAY_ANY, 1449964800},
  // month+mday+minute
  // 2015 Dec 13, 00:45:00
  { 11,12,-1,45, WDAY_ANY, 1449967500},
  // month+mday+hour
  // 2015 Dec 13, 13:00:00
  { 11,12,13,-1, WDAY_ANY, 1450011600},
  // month+mday+hour+minute
  // 2015 Dec 13, 13:45:00
  { 11,12,13,45, WDAY_ANY, 1450014300},

  //////// 'past' time finding
  // minute
  // 2015 Nov 12, 13:23:00
  { -1,-1,-1,23, WDAY_ANY, 1447334580},
  // hour
  // 2015 Nov 13, 11:00:00
  { -1,-1,11,-1, WDAY_ANY, 1447412400},
  // day
  // 2015 Dec 11, 00:00:00
  { -1,10,-1,-1, WDAY_ANY, 1449792000},
  // month
  // 2016 Oct  1, 00:00:00
  {  9,-1,-1,-1, WDAY_ANY, 1475280000},
  // month+hour
  // 2016 Oct  1, 12:00:00
  {  9,-1,12,-1, WDAY_ANY, 1475323200},

  //////// wday time finding
  // now, -Th
  // 2015 Nov 13, 00:00:00
  { -1,-1,-1,-1, WDAY_ANY & ~WDAY_THURSDAY, 1447372800},
  // now, -Th-Fr
  // 2015 Nov 14, 00:00:00
  { -1,-1,-1,-1, WDAY_ANY & ~(WDAY_THURSDAY|WDAY_FRIDAY), 1447459200},
  // now, -Th-Fr-Sa
  // 2015 Nov 15, 00:00:00
  { -1,-1,-1,-1, WDAY_ANY & ~(WDAY_THURSDAY|WDAY_FRIDAY|WDAY_SATURDAY), 1447545600},
  // now, -Th-Fr-Sa-Su
  // 2015 Nov 16, 00:00:00
  { -1,-1,-1,-1, WDAY_MONDAY|WDAY_TUESDAY|WDAY_WEDNESDAY, 1447632000},
  // now, -Th-Fr-Sa-Su-Mo
  // 2015 Nov 17, 00:00:00
  { -1,-1,-1,-1, WDAY_TUESDAY|WDAY_WEDNESDAY, 1447718400},
  // now, -Th-Fr-Sa-Su-Mo-Tu
  // 2015 Nov 18, 00:00:00
  { -1,-1,-1,-1, WDAY_WEDNESDAY, 1447804800},
  // now, -We
  // now
  { -1,-1,-1,-1, WDAY_ANY & ~WDAY_WEDNESDAY, s_2015_nov12_123456_gmt},
  // now, wday=0
  // now
  { -1,-1,-1,-1, 0, s_2015_nov12_123456_gmt},

  //////// wday+ time finding
  // 19th, -Th
  // 2015 Nov 20, 00:00:00
  { -1,18,-1,-1, WDAY_ANY & ~WDAY_THURSDAY, 1447977600},
  // Dec, -Tu
  // 2015 Dec  2, 00:00:00
  { 11,-1,-1,-1, WDAY_ANY & ~WDAY_TUESDAY, 1449014400},

  //////// 'bogus' time finding
  // minute
  // 2015 Nov 12, 12:60:00 = 2015 Nov 12, 13:00:00
  { -1,-1,-1,60, WDAY_ANY, 1447333200},
  // hour
  // 2015 Nov 12, 24:00:00 = 2015 Nov 13, 00:00:00
  { -1,-1,24,-1, WDAY_ANY, 1447372800},
  // mday
  // 2015 Nov 33, 00:00:00 = 2015 Dec  3, 00:00:00
  { -1,32,-1,-1, WDAY_ANY, 1449100800},
  // month
  // 2015 Month13 1, 00:00:00 = 2016 Jan  1, 00:00:00
  { 12,-1,-1,-1, WDAY_ANY, 1451606400},

  // Sentinel
  { 0,0,0,0, 0, 0},
};

void test_cron__simples(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = CRON_MINUTE_ANY,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,

    .may_be_instant = true,
  };
  for (int i = 0; ; i++) {
    if (s_cron_test_info[i].dest_time == 0) {
      break;
    }
    test_cron.minute = s_cron_test_info[i].minute;
    test_cron.hour = s_cron_test_info[i].hour;
    test_cron.mday = s_cron_test_info[i].mday;
    test_cron.month = s_cron_test_info[i].month;
    test_cron.wday = s_cron_test_info[i].wday;
    time_t base = s_2015_nov12_123456_gmt;
    time_t advance = s_cron_test_info[i].dest_time - base;
    // DST off
    prv_basic_test(&s_timezone_gmt, &test_cron, base, advance, advance, 0);
    // DST on
    base -= SECONDS_PER_HOUR;
    prv_basic_test(&s_timezone_gmt, &test_cron, base, advance, advance, 2);
  }
}

void test_cron__dst_simple_to(void) {
  // Nov 21st, 01:00:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 0,
    .hour = 1,
    .mday = 20,
    .month = 10,

    .may_be_instant = true,
  };
  // 2015 Nov 21, 00:00:00 GMT
  const time_t advance = 1448064000 - s_2015_nov12_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, advance, advance, 1);
}

void test_cron__dst_simple_from(void) {
  // Dec 21st, 01:00:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 0,
    .hour = 1,
    .mday = 20,
    .month = 11,

    .may_be_instant = true,
  };
  // 2015 Dec 21, 01:00:00 GMT
  const time_t advance = 1450659600 - s_2015_dec19_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_dec19_123456_gmt, advance, advance, 1);
}

void test_cron__dst_rollover_to(void) {
  // Nov 20th, 03:00:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 0,
    .hour = 3,
    .mday = 19,
    .month = 10,

    .may_be_instant = true,
  };
  // 2015 Nov 20, 02:00:00 GMT
  const time_t advance = 1447984800 - s_2015_nov12_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, advance, advance, 1);
}

void test_cron__dst_rollover_from(void) {
  // Dec 20th, 02:00:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 0,
    .hour = 2,
    .mday = 19,
    .month = 11,

    .may_be_instant = true,
  };
  // 2015 Dec 20, 02:00:00 GMT
  time_t advance = 1450576800 - s_2015_dec19_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_dec19_123456_gmt, advance, advance, 1);
}

void test_cron__dst_hole_to(void) {
  // NOTE: This behavior is SUPER weird, and it could change in the future.
  // A failure in this test is not necessarily a problem.

  // Nov 20th, 02:30:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 2,
    .mday = 19,
    .month = 10,

    .may_be_instant = true,
  };
  // 2015 Nov 20, 02:00:00 GMT (DST start)
  const time_t advance = 1447984800 - s_2015_nov12_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, advance, advance, 1);
}

void test_cron__dst_hole_from(void) {
  // NOTE: This behavior is SUPER weird, and it could change in the future.
  // A failure in this test is not necessarily a problem.

  // Dec 20th, 01:30:00 local
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 1,
    .mday = 19,
    .month = 11,

    .may_be_instant = true,
  };
  // 2015 Dec 20, 00:30:00 GMT (the 'first' 1:30)
  const time_t advance = 1450571400 - s_2015_nov12_123456_gmt;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, advance, advance, 1);
}

static void prv_counting_cb(CronJob *job, void *cb_data) {
  static int s_counter = 0;
  job->cb_data = (void*)((uintptr_t)++s_counter);
}

#define CRON_JOB(min, hr, day, mo, callback) \
  { \
    .cb = callback, \
    .cb_data = (void*)0, \
    .minute = min, \
    .hour = hr, \
    .mday = day, \
    .month = mo, \
    .may_be_instant = true, \
  },

void test_cron__scheduled_after(void) {
  CronJob jobs[] = {
    CRON_JOB(CRON_MINUTE_ANY, CRON_HOUR_ANY, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_counting_cb)
    CRON_JOB(CRON_MINUTE_ANY, CRON_HOUR_ANY, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(1, CRON_HOUR_ANY, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(3, CRON_HOUR_ANY, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(10, CRON_HOUR_ANY, 1, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(25, CRON_HOUR_ANY, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(55, 1, CRON_MDAY_ANY, CRON_MONTH_ANY, prv_cron_callback)
    CRON_JOB(CRON_MINUTE_ANY, CRON_HOUR_ANY, 1, CRON_MONTH_ANY, prv_cron_callback)
  };

  CronJob new_job = {
    .cb = prv_counting_cb,
    .cb_data = (void*)0,
  };

  prv_set_rtc(s_2015_nov12_123456_gmt, &s_timezone_gmt);

  cron_clear_all_jobs();
  cl_assert_equal_i(cron_service_get_job_count(), 0);

  for (int i = 0; i < ARRAY_LENGTH(jobs); ++i) {
    cron_job_schedule(&jobs[i]);
  }
  cron_job_schedule_after(&jobs[0], &new_job);

  cl_assert_equal_i((uintptr_t)jobs[0].cb_data, 0);
  cl_assert_equal_i((uintptr_t)new_job.cb_data, 0);
  cl_assert_equal_i(cron_service_get_job_count(), ARRAY_LENGTH(jobs) + 1);

  fake_rtc_increment_time(0);

  cron_service_wakeup();
  cl_assert_equal_i((uintptr_t)jobs[0].cb_data, 1);
  cl_assert_equal_i((uintptr_t)new_job.cb_data, 2);
  cl_assert_equal_i(cron_service_get_job_count(), 6);

  fake_rtc_increment_time(SECONDS_PER_DAY * 60);
  cron_service_wakeup();
  cl_assert_equal_i(cron_service_get_job_count(), 0);
}

void test_cron__offset_negative_seconds_one_wday(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 0,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = -SECONDS_PER_DAY,

    .wday = WDAY_FRIDAY,
    .may_be_instant = false,
  };

  const time_t advance = 30 * SECONDS_PER_MINUTE;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 1);
}

void test_cron__offset_negative_seconds_any_day(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 0,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = -SECONDS_PER_DAY,

    .may_be_instant = false,
  };

  const time_t advance = 30 * SECONDS_PER_MINUTE;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 1);
}

void test_cron__offset_positive_seconds_one_wday(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 0,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = SECONDS_PER_DAY,

    .wday = WDAY_THURSDAY,
    .may_be_instant = false,
  };

  const time_t advance = 30 * SECONDS_PER_MINUTE + SECONDS_PER_DAY;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 1);
}

void test_cron__offset_positive_seconds_any_day(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 0,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = SECONDS_PER_DAY,

    .may_be_instant = false,
  };

  const time_t advance = 30 * SECONDS_PER_MINUTE;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 1);
}

void test_cron__offset_negative_seconds_every_second(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = CRON_MINUTE_ANY,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = -SECONDS_PER_MINUTE,

    .may_be_instant = true,
  };

  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, 0, 0, 0);
}

void test_cron__offset_positive_seconds_every_second(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = CRON_MINUTE_ANY,
    .hour = CRON_HOUR_ANY,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = SECONDS_PER_MINUTE,

    .may_be_instant = true,
  };

  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_123456_gmt, 0, 0, 0);
}

void test_cron__offset_negative_seconds_any_day_dst(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 1,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = -30 * SECONDS_PER_MINUTE,

    .may_be_instant = false,
  };

  const time_t advance = SECONDS_PER_DAY;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 2);
}

void test_cron__offset_positive_seconds_any_day_dst(void) {
  CronJob test_cron = {
    .cb = prv_cron_callback,
    .cb_data = (void*)0,

    .minute = 30,
    .hour = 0,
    .mday = CRON_MDAY_ANY,
    .month = CRON_MONTH_ANY,
    .offset_seconds = 30 * SECONDS_PER_MINUTE,

    .may_be_instant = false,
  };

  const time_t advance = SECONDS_PER_DAY;
  prv_basic_test(&s_timezone_gmt, &test_cron, s_2015_nov12_000000_gmt, advance, advance, 2);
}

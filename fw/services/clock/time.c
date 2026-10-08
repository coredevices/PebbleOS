/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/types.h>
#include <pbl/drivers/rtc.h>
#include <syscall/syscall_internal.h>
#include <pbl/services/time.h>
#include <pbl/util/units.h>
#include <string.h>
#include <pbl/util/time.h>

// timezone abbreviation
static char s_timezone_abbr[TZ_LEN] = {0}; // longest timezone abbreviation is 5 char + null
static int32_t s_timezone_gmtoffset = 0;
static int32_t s_dst_adjust = PBL_SEC_PER_HOUR;
static time_t s_dst_start = 0;
static time_t s_dst_end = 0;

int32_t time_get_gmtoffset(void) {
  return s_timezone_gmtoffset;
}

bool time_get_isdst(time_t utc_time) {
  // do we have any DST set for the timezone we are in
  if ((s_dst_start == 0) || (s_dst_end == 0)) {
    return false;
  }

  return ((s_dst_start <= utc_time) && (utc_time < s_dst_end));
}

int32_t time_get_dstoffset(void) {
  return s_dst_adjust;
}

time_t time_get_dst_start(void) {
  return s_dst_start;
}

DEFINE_SYSCALL(time_t, sys_time_utc_to_local, time_t t) {
  return time_utc_to_local(t);
}

time_t time_utc_to_local(time_t utc_time) {
  utc_time += time_get_isdst(utc_time) ? s_dst_adjust : 0;
  utc_time += s_timezone_gmtoffset;
  return utc_time;
}

time_t time_local_to_utc(time_t local_time) {
  // Note that there is 1 hour a year where it is impossible to undo the DST offset based solely
  // on local time. For example, if the clock goes backward by 1 hour at 2am, then all times
  // between 1am and 2am will appear twice, and there is no way to tell which of the two
  // intervals we are being passed.
  local_time -= s_timezone_gmtoffset;
  local_time -= time_get_isdst(local_time - s_dst_adjust) ? s_dst_adjust : 0;
  return local_time;
}

void time_get_timezone_abbr(char *out_buf, time_t utc_time) {
  if (!out_buf) {
    return;
  }
  strncpy(out_buf, s_timezone_abbr, TZ_LEN);
  out_buf[TZ_LEN - 1] = 0;

  // Timezones with daylight savings, update modifier with current dst char
  // ie. P*T is PDT for daylight savings, PST for non-daylight savings
  char *tz_zone_dst_char = memchr(out_buf, '*', TZ_LEN);
  if (tz_zone_dst_char) {
    *tz_zone_dst_char = (time_get_isdst(utc_time)) ? 'D' : 'S';
    // Workaround for UK Winter, Greenwich Mean Time; UK Summer, British Summer Time
    if (!strncmp(out_buf, "BDT", TZ_LEN)) {
      strncpy(out_buf, "BST", TZ_LEN);
    } else if (!strncmp(out_buf, "BST", TZ_LEN)) {
      strncpy(out_buf, "GMT", TZ_LEN);
    }
  }
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
  const time_t utc_time = *timep;
  result->tm_isdst = time_get_isdst(utc_time);
  result->tm_gmtoff = time_get_gmtoffset() + (result->tm_isdst ? s_dst_adjust : 0);
  time_get_timezone_abbr(result->tm_zone, utc_time);
  pbl_time_breakdown(utc_time + result->tm_gmtoff, result);
  return result;
}

void time_util_update_timezone(const TimezoneInfo *tz_info) {
  strncpy(s_timezone_abbr, tz_info->tm_zone, sizeof(tz_info->tm_zone) + 0);
  s_timezone_abbr[TZ_LEN - 1] = '\0';
  s_timezone_gmtoffset = tz_info->tm_gmtoff;
  s_dst_start = tz_info->dst_start;
  s_dst_end = tz_info->dst_end;
  // Lord Howe Island has a half-hour DST
  if (tz_info->dst_id == DSTID_LORDHOWE) {
    s_dst_adjust = PBL_SEC_PER_HOUR / 2;
  } else {
    s_dst_adjust = PBL_SEC_PER_HOUR;
  }
}

time_t time_util_get_midnight_of(time_t ts) {
  struct tm tm;
  localtime_r(&ts, &tm);
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  return mktime(&tm);
}

bool time_util_range_spans_day(time_t start, time_t end, time_t start_of_day) {
  return (start <= start_of_day && end >= (start_of_day + PBL_SEC_PER_DAY));
}

time_t time_util_utc_to_local_offset(void) {
  time_t now = rtc_get_time();
  return (time_utc_to_local(now) - now);
}

// ---------------------------------------------------------------------------------------
enum pbl_weekday time_util_get_day_in_week(time_t utc_sec) {
  struct tm local_tm;
  localtime_r(&utc_sec, &local_tm);
  return local_tm.tm_wday;
}

// ---------------------------------------------------------------------------------------
uint16_t time_util_get_day(time_t utc_sec) {
  // Convert to local seconds
  time_t local_sec = time_utc_to_local(utc_sec);

  // Figure out the day index
  return (local_sec / PBL_SEC_PER_DAY);
}

// ---------------------------------------------------------------------------------------
int time_util_get_minute_of_day(time_t utc_sec) {
  struct tm local_tm;
  localtime_r(&utc_sec, &local_tm);
  return (local_tm.tm_hour * PBL_MIN_PER_HOUR) + local_tm.tm_min;
}

// ---------------------------------------------------------------------------------------
time_t time_start_of_today(void) {
  time_t now = rtc_get_time();
  return time_util_get_midnight_of(now);
}

DEFINE_SYSCALL(time_t, sys_time_start_of_today, void) {
  return time_start_of_today();
}

// ---------------------------------------------------------------------------------------
uint32_t time_get_uptime_seconds(void) {
  RtcTicks ticks = rtc_get_ticks();
  return ticks / PBL_TICK_HZ;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/time.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// DST special cases. These map to indexes in the tools/timezones.py script that handles parsing
// the olsen database into a compressed form. Don't change these without changing the script.
//
// Note that we don't correctly handle Morroco's DST rules, they're incredibly complex due to them
// suspending DST each year for Ramadan, resulting in 4 DST transitions each year.
//
// Any DST ids that aren't listed below have sane DST rules, where they change to DST in the
// spring on the same day by 1 hour each year and change from DST on a later day each year.
#define DSTID_BRAZIL   6
#define DSTID_LORDHOWE 20

//! Minimal struct to store timezone info in RTC registers
typedef struct TimezoneInfo {
  char tm_zone[TZ_LEN - 1]; //!< Up to 5 character (no null terminator) timezone abbreviation
  uint8_t dst_id;           //!< Daylight savings time zone index
  int16_t timezone_id;      //!< Olson index of timezone
  int32_t tm_gmtoff;        //!< GMT time offset
  time_t dst_start;         //!< timestamp of start of daylight savings period (0 if none)
  time_t dst_end;           //!< timestamp of end of daylight savings period (0 if none)
} TimezoneInfo;

//! Provides the timezone abbreviation string for the given time. Uses the utc_time provided
//! to correct the abbreviation for daylight savings time if applicable
//! @param out_buf should have length TZ_LEN
//! @param utc_time time used to determine whether daylight savings applies
void time_get_timezone_abbr(char *out_buf, time_t utc_time);

//! Provides the gmt offset
int32_t time_get_gmtoffset(void);

//! Returns true if the UNIX time provided falls within DST
bool time_get_isdst(time_t utc_time);

//! Returns the DST offset
int32_t time_get_dstoffset(void);

//! Returns the DST start timestamp
time_t time_get_dst_start(void);

//! Convert UTC time, as returned by rtc_get_time() into local time
time_t time_utc_to_local(time_t utc_time);

//! Convert local time to UTC time
time_t time_local_to_utc(time_t local_time);

//! Set the timezone
void time_util_update_timezone(const TimezoneInfo *tz_info);

time_t time_util_get_midnight_of(time_t ts);

bool time_util_range_spans_day(time_t start, time_t end, time_t start_of_day);

//! User-mode access calls
time_t sys_time_utc_to_local(time_t t);

time_t time_util_utc_to_local_offset(void);

//! Computes the day index from UTC seconds. This index should change every day at midnight local
//! time
//! @param utc_sec Time to retrieve index for
//! @return Day index
uint16_t time_util_get_day(time_t utc_sec);

enum pbl_weekday time_util_get_day_in_week(time_t utc_sec);

//! Computes the minute of the day
//! @param utc_sec Time to retrieve minute for
//! @return Minute of day
int time_util_get_minute_of_day(time_t utc_sec);

//! Return the UTC time that corresponds to the start of today (midnight).
//! @return the UTC time corresponding to the start of today (midnight)
time_t time_start_of_today(void);

//! Return the number of seconds since the system was restarted. This time is based on the
//! tickcount and so, unlike rtc_get_time(), it won't be affected if the phone changes the UTC
//! time on the watch.
uint32_t time_get_uptime_seconds(void);

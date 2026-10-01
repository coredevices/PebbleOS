/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PBL_TM_YEAR_ORIGIN 1900
#define PBL_EPOCH_YEAR     1970
#define PBL_EPOCH_WDAY     4

enum pbl_weekday {
  PBL_SUNDAY = 0,
  PBL_MONDAY,
  PBL_TUESDAY,
  PBL_WEDNESDAY,
  PBL_THURSDAY,
  PBL_FRIDAY,
  PBL_SATURDAY,
};

static inline bool pbl_time_is_weekday(enum pbl_weekday day) {
  return (day >= PBL_MONDAY) && (day <= PBL_FRIDAY);
}

static inline bool pbl_time_is_weekend(enum pbl_weekday day) {
  return (day == PBL_SATURDAY) || (day == PBL_SUNDAY);
}

bool pbl_time_is_leap_year(int year);

//! @param month Month of the year, 1 (January) to 12 (December)
//! @param is_leap_year Whether the year is a leap year
int pbl_time_days_in_month(int month, bool is_leap_year);

void pbl_time_split_seconds(uint32_t seconds, uint32_t *days, uint32_t *hours, uint32_t *minutes,
                            uint32_t *secs);

//! @return hour (0-23) as shown on a 24h clock, or as 1-12 on a 12h clock
int pbl_time_display_hour(int hour, bool is_24h);

//! Adds delta to a minute of the day, wrapping around midnight.
int pbl_time_minute_of_day_adjust(int minute, int delta);

//! Fills the calendar fields of tm (tm_sec to tm_yday) from seconds since the epoch. The DST,
//! offset and zone fields are left untouched.
void pbl_time_breakdown(time_t t, struct tm *tm);

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <time.h>

/**
 * @defgroup util_time Calendar time
 * @ingroup util
 * @brief Calendar arithmetic independent of the time zone and the RTC.
 * @{
 */

/** @brief Year that @c tm_year in struct tm counts from. */
#define PBL_TM_YEAR_ORIGIN 1900
/** @brief Year of the Unix epoch. */
#define PBL_EPOCH_YEAR 1970
/** @brief Day of the week of the Unix epoch (Thursday), as an enum pbl_weekday value. */
#define PBL_EPOCH_WDAY 4

/** @brief Day of the week, numbered like @c tm_wday. */
enum pbl_weekday {
  /** Sunday. */
  PBL_SUNDAY = 0,
  /** Monday. */
  PBL_MONDAY,
  /** Tuesday. */
  PBL_TUESDAY,
  /** Wednesday. */
  PBL_WEDNESDAY,
  /** Thursday. */
  PBL_THURSDAY,
  /** Friday. */
  PBL_FRIDAY,
  /** Saturday. */
  PBL_SATURDAY,
};

/**
 * @brief Check whether a day is a working day (Monday to Friday).
 *
 * @param day Day of the week.
 * @return true for Monday to Friday.
 */
static inline bool pbl_time_is_weekday(enum pbl_weekday day) {
  return (day >= PBL_MONDAY) && (day <= PBL_FRIDAY);
}

/**
 * @brief Check whether a day is on the weekend (Saturday or Sunday).
 *
 * @param day Day of the week.
 * @return true for Saturday and Sunday.
 */
static inline bool pbl_time_is_weekend(enum pbl_weekday day) {
  return (day == PBL_SATURDAY) || (day == PBL_SUNDAY);
}

/**
 * @brief Check whether a Gregorian year is a leap year.
 *
 * @param year Full year, e.g. 2024.
 * @return true for a leap year.
 */
bool pbl_time_is_leap_year(int year);

/**
 * @brief Get the number of days in a month.
 *
 * @param month Month of the year, 1 (January) to 12 (December).
 * @param is_leap_year Whether the year is a leap year.
 * @return Number of days, 28 to 31.
 */
int pbl_time_days_in_month(int month, bool is_leap_year);

/**
 * @brief Split a duration into days, hours, minutes and seconds.
 *
 * @param seconds Duration in seconds.
 * @param[out] days Whole days.
 * @param[out] hours Remaining hours, 0 to 23.
 * @param[out] minutes Remaining minutes, 0 to 59.
 * @param[out] secs Remaining seconds, 0 to 59.
 */
void pbl_time_split_seconds(uint32_t seconds, uint32_t *days, uint32_t *hours, uint32_t *minutes,
                            uint32_t *secs);

/**
 * @brief Convert an hour of the day to the hour shown on a clock.
 *
 * @param hour Hour of the day, 0 to 23.
 * @param is_24h Whether the clock uses the 24-hour format.
 * @return @p hour on a 24-hour clock, or 1 to 12 on a 12-hour clock.
 */
int pbl_time_display_hour(int hour, bool is_24h);

/**
 * @brief Add a delta to a minute of the day, wrapping around midnight.
 *
 * @param minute Minute of the day, 0 to 1439.
 * @param delta Minutes to add, at most one day in either direction.
 * @return Adjusted minute of the day, 0 to 1439.
 */
int pbl_time_minute_of_day_adjust(int minute, int delta);

/**
 * @brief Fill the calendar fields of a struct tm from seconds since the epoch.
 *
 * Sets @c tm_sec to @c tm_yday. The DST, offset and zone fields are left untouched.
 *
 * @param t Seconds since the Unix epoch.
 * @param[out] tm Broken-down time.
 */
void pbl_time_breakdown(time_t t, struct tm *tm);

/** @} */

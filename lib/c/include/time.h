/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h> // time_t and size_t

#define TZ_LEN 6

//! @file time.h

//! @addtogroup StandardC Standard C
//! @{
//!   @addtogroup StandardTime Time
//! \brief Standard system time functions
//!
//! This module contains standard time functions and formatters for printing.
//! Note that Pebble now supports both local time and UTC time
//! (including timezones and daylight savings time).
//! Most of these functions are part of the C standard library which is documented at
//! https://sourceware.org/newlib/libc.html#Timefns
//! @{

//! structure containing broken-down time for expressing calendar time
//! (ie. Year, Month, Day of Month, Hour of Day) and timezone information
struct tm {
  int tm_sec;   /*!< Seconds. [0-60] (1 leap second) */
  int tm_min;   /*!< Minutes. [0-59] */
  int tm_hour;  /*!< Hours.  [0-23] */
  int tm_mday;  /*!< Day. [1-31] */
  int tm_mon;   /*!< Month. [0-11] */
  int tm_year;  /*!< Years since 1900 */
  int tm_wday;  /*!< Day of week. [0-6] */
  int tm_yday;  /*!< Days in year.[0-365] */
  int tm_isdst; /*!< DST. [-1/0/1] */

  int tm_gmtoff; /*!< Total seconds east of UTC, DST included. tm_isdst is an indicator only --
                      never add it to this value. */
  char tm_zone[TZ_LEN]; /*!< Timezone abbreviation */
};

//! Obtain the number of seconds and milliseconds part since the epoch.
//!   This is a non-standard C function provided for convenience.
//! @param tloc if provided receives the current UTC Unix Time seconds portion
//! @param out_ms if provided receives the current Unix Time milliseconds portion
//! @return Current Unix Time milliseconds portion
uint16_t time_ms(time_t *tloc, uint16_t *out_ms);

//!   @} // end addtogroup StandardTime
//! @} // end addtogroup StandardC

// The below standard c time functions are documented for the SDK
// by their applib pbl_override wrappers in pbl_std.h

struct tm *localtime(const time_t *timep);

struct tm *gmtime(const time_t *timep);

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

time_t mktime(struct tm *tb);

struct tm *gmtime_r(const time_t *timep, struct tm *result);

struct tm *localtime_r(const time_t *timep, struct tm *result);

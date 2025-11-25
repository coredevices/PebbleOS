/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "util/time/time.h"

/*
 * C Standard Library functions for consumption by 3rd party apps
 *
 */

//! @addtogroup StandardC Standard C
//! @{
//!   @addtogroup StandardTime Time
//! @{

//! Obtain the number of seconds since epoch.
//! Note that the epoch is not adjusted for Timezones and Daylight Savings.
//! @param tloc Optionally points to an address of a time_t variable to store the time in.
//!     If you only want to use the return value, you may pass NULL into tloc instead
//! @return The number of seconds since epoch, January 1st 1970
time_t pbl_override_time(time_t *tloc);

//! Obtain the number of seconds elapsed between beginning and end represented as a double.
//! @param end A time_t variable representing some number of seconds since epoch, January 1st 1970
//! @param beginning A time_t variable representing some number of seconds since epoch,
//!     January 1st 1970. Note that end should be greater than beginning, but this is not enforced.
//! @return The number of seconds elapsed between beginning and end.
//! @note Pebble uses software floating point emulation.  Including this function which returns a
//!     double will significantly increase the size of your binary.  We recommend directly
//!     subtracting both timestamps to calculate a time difference.
//!     \code{.c}
//!     int difference = ts1 - ts2;
//!     \endcode
double pbl_override_difftime(time_t end, time_t beginning);

//! Obtain the number of seconds since epoch.
//! Note that the epoch is adjusted for Timezones and Daylight Savings.
//! @param tloc Optionally points to an address of a time_t variable to store the time in.
//!     If you only want to use the return value, you may pass NULL into tloc instead
//! @return The number of seconds since epoch, January 1st 1970
time_t pbl_override_time_legacy(time_t *tloc);

//! convert the time value pointed at by clock to a struct tm which contains the time
//! adjusted for the local timezone
//! @param timep A pointer to an object of type time_t that contains a time value
//! @return A pointer to a struct tm containing the broken out time value adjusted
//!   for the local timezone
struct tm *pbl_override_localtime(const time_t *timep);

//! convert the time value pointed at by clock to a struct tm
//!   which contains the time expressed in Coordinated Universal Time (UTC)
//! @param timep A pointer to an object of type time_t that contains a time value
//! @return A pointer to a struct tm containing Coordinated Universal Time (UTC)
struct tm *pbl_override_gmtime(const time_t *timep);

//! convert the broken-down time structure to a timestamp
//!   expressed in Coordinated Universal Time (UTC)
//! @param tb A pointer to an object of type tm that contains broken-down time
//! @return The number of seconds since epoch, January 1st 1970
time_t pbl_override_mktime(struct tm *tb);

//! Returns the current local time in Unix Timestamp Format with milliseconds
//! The time_ms() method, in contrast, returns the UTC time instead of local time.
//! @param t_loc Optionally points to an address of a time_t variable to store the time in.
//!     If you don't need this value, you may pass NULL into t_loc instead
//! @param out_ms Optionally points to an address of a uin16_t variable to store the ms in.
//!     If you only want to use the return value, you may pass NULL into out_ms instead
//! @return Current milliseconds portion
uint16_t pbl_override_time_ms_legacy(time_t *t_loc, uint16_t *out_ms);

//! Format the time value at tm according to fmt and place the result in a buffer s of size max
//! @param s A preallocation char array of size max
//! @param maxsize the size of the array s
//! @param format a formatting string
//! @param tm_p A pointer to a struct tm containing a broken out time value
//! @return The number of bytes placed in the array s, not including the null byte,
//!   0 if the value does not fit.
int pbl_strftime(char* s, size_t maxsize, const char* format, const struct tm* tm_p);

//! Returns the current UTC time in Unix Timestamp Format with Milliseconds
//!     @param t_utc if provided receives current UTC Unix Time seconds portion
//!     @param out_ms if provided receives current Unix Time milliseconds portion
//!     @return Current Unix Time milliseconds portion
uint16_t time_ms(time_t *t_utc, uint16_t *out_ms);

//!   @} // end addtogroup StandardTime
//! @} // end addtogroup StandardC

int pbl_snprintf(char * str, size_t n, const char * format, ...);

void *pbl_memcpy(void *destination, const void *source, size_t num);

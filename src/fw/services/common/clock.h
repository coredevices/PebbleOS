/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <inttypes.h>
#include "util/time/time.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup WallTime Wall Time
//!   \brief Functions, data structures and other things related to wall clock time.
//!
//! This module contains utilities to get the current time and create strings with formatted
//! dates and times.
//!   @{

//! The maximum length for a timezone full name (e.g. America/Chicago)
#define TIMEZONE_NAME_LENGTH 32
// Large enough for common usages like "Wednesday" or "30 minutes ago"
#define TIME_STRING_REQUIRED_LENGTH 20
// Large enough for time. e.g. 14:20
#define TIME_STRING_TIME_LENGTH 10
// Large enough for day/mo e.g. 04/27
#define TIME_STRING_DATE_LENGTH 10
// Large enough for day e.g. 27
#define TIME_STRING_DAY_DATE_LENGTH 3

//! Weekday values
typedef enum {
  TODAY = 0,  //!< Today
  SUNDAY,     //!< Sunday
  MONDAY,     //!< Monday
  TUESDAY,    //!< Tuesday
  WEDNESDAY,  //!< Wednesday
  THURSDAY,   //!< Thursday
  FRIDAY,     //!< Friday
  SATURDAY,   //!< Saturday
} WeekDay;

//! @internal
//! Initialize clock service
void clock_init(void);

//! @internal
void clock_get_time_tm(struct tm* time_tm);

//! @internal
//! @param add_space whether to add a space between the time and AM/PM
size_t clock_format_time(char *buffer, uint8_t size, int16_t hours, int16_t minutes,
                         bool add_space);

//! Same as \ref clock_copy_time_string, but with a supplied timestamp
size_t clock_copy_time_string_timestamp(char *buffer, uint8_t size, time_t timestamp);

//! Copies a time string into the buffer, formatted according to the user's time display preferences (such as 12h/24h
//! time).
//! Example results: "7:30" or "15:00".
//! @note AM/PM are also outputted with the time if the user's preference is 12h time.
//! @param[out] buffer A pointer to the buffer to copy the time string into
//! @param size The maximum size of buffer
void clock_copy_time_string(char *buffer, uint8_t size);

//! Gets the time formatted as "7:30" or "15:00" depending on the user's 12/24h clock setting
//! @note AM/PM is not outputted. Use in combination with \ref clock_get_time_word.
size_t clock_get_time_number(char *buffer, size_t buffer_size, time_t timestamp);

//! Gets AM/PM or sets the first character to '\0' depending on the user's 12/24h clock setting
//! @note Use in combination with \ref clock_get_time_number to get a full hour minute timestamp
size_t clock_get_time_word(char *buffer, size_t buffer_size, time_t timestamp);

//! Get the relative time string of an event, e.g. "10 min. ago", with "10" and " min ago"
//! copied into separate buffers so they can be rendered in different fonts
void clock_get_event_relative_time_string(char *number_buffer, int number_buffer_size,
    char *word_buffer, int word_buffer_size, time_t timestamp, uint16_t duration,
    time_t current_day, bool all_day);

//! Gets the user's 12/24h clock style preference.
//! @return `true` if the user prefers 24h-style time display or `false` if the
//! user prefers 12h-style time display.
bool clock_is_24h_style(void);

//! @internal
//! Sets the user's time display style.
//! @param is_24h_style True means 24h style, false means 12h style.
void clock_set_24h_style(bool is_24h_style);

//! Checks if timezone is currently set, otherwise gmtime == localtime.
//! @return `true` if timezone has been set, false otherwise
bool clock_is_timezone_set(void);

//! @internal
//! Checks the timezone source. If the source is manual, the user must select the timezone from
//! the settings menu, if the source is automatic the timezone will be set by the phone.
//! @return true if the user has a manual timezone set, false if the timezone is set by the phone
bool clock_timezone_source_is_manual(void);

//! @internal
//! Sets the timezone source. If the source is manual, the user must select the timezone from
//! the settings menu, if the source is automatic the timezone will be set by the phone.
//! @param manual True means a manually selected timezone will be used,
//!               false means the phone's timezone will be used
void clock_set_manual_timezone_source(bool manual);

//! @internal
//! If timezone is set, copies the current timezone long name (e.g. America/Chicago)
//! to buffer region_name.
//! @param timezone A pointer to the buffer to copy the timezone long name into
//! @param buffer_size Size of the allocated buffer to copy the timezone long name into
//! @note region_name size should be at least TIMEZONE_NAME_LENGTH bytes
void clock_get_timezone_region(char *region_name, const size_t buffer_size);

//! Function to retrieve the current timezone's region_id
//! @return the index of the current timezone in terms of the timezone database
int16_t clock_get_timezone_region_id(void);

//! Function to set the watch to the selected timezone region_id
//! @param region_id the index of the selected timezone in terms of the timezone database
void clock_set_timezone_by_region_id(uint16_t region_id);

//! Converts a (day, hour, minute) specification to a UTC timestamp occurring in the future
//! Always returns a timestamp for the next occurring instance,
//! example: specifying TODAY@14:30 when it is 14:40 will return a timestamp for 7 days from
//! now at 14:30
//! @note This function does not support Daylight Saving Time (DST) changes, events scheduled
//! during a DST change will be off by an hour.
//! @param day WeekDay day of week including support for specifying TODAY
//! @param hour hour specified in 24-hour format [0-23]
//! @param minute minute [0-59]
time_t clock_to_timestamp(WeekDay day, int hour, int minute);

//! Get a friendly date out of a timestamp (e.g. "Today", "Tomorrow")
//! @param buffer buffer to output the friendly date into
//! @param buf_size size of the buffer
//! @param timestamp timestamp to get a friendly date for
void clock_get_friendly_date(char *buffer, int buf_size, time_t timestamp);

//! Get a friendly "time since" out of a timestamp (e.g. "Just now", "5 minutes ago")
//! @param buffer buffer to output the friendly time into
//! @param buf_size size of the buffer
//! @param timestamp timestamp to get a friendly time for
void clock_get_since_time(char *buffer, int buf_size, time_t timestamp);

//! Get a friendly "time to" out of a timestamp (e.g. "Now", "In 5 hours")
//! @param buffer buffer to output the friendly time into
//! @param buf_size size of the buffer
//! @param timestamp timestamp to get a friendly time for
//! @param max_relative_hrs how many hours for which it should show "IN X HOURS"
void clock_get_until_time(char *buffer, int buf_size, time_t timestamp,
                          int max_relative_hrs);

//! Get a friendly "time to" out of a timestamp, without ever writing the real time
//! @param buffer buffer to output the friendly time into
//! @param buf_size size of the buffer
//! @param timestamp timestamp to get a friendly time for
//! @param max_relative_hrs how many hours for which it should show "IN X HOURS"
void clock_get_until_time_without_fulltime(char *buffer, int buf_size, time_t timestamp,
                                          int max_relative_hrs);

//! Get the date in MM/DD format
size_t clock_get_date(char *buffer, int buf_size, time_t timestamp);

//! Get the day date in DD format
size_t clock_get_day_date(char *buffer, int buf_size, time_t timestamp);

//! Get the date in Month DD format (e.g. "July 16")
size_t clock_get_month_named_date(char *buffer, size_t buffer_size, time_t timestamp);

//! Get the date in Mon DD format (e.g. "Jul 16")
size_t clock_get_month_named_abbrev_date(char *buffer, size_t buffer_size, time_t timestamp);

//! Get a friendly capitalized "time to" out of a timestamp (e.g. "NOW", "IN 5 HOURS")
//! @param buffer buffer to output the friendly time into
//! @param buf_size size of the buffer
//! @param timestamp timestamp to get a friendly time for
//! @param max_relative_hrs how many hours for which it should show "IN X HOURS"
void clock_get_until_time_capitalized(char *buffer, int buf_size, time_t timestamp,
                                      int max_relative_hrs);

//!   @} // end addtogroup WallTime
//! @} // end addtogroup Foundation

//! Get a textual daypart message out of a timestamp and time offset (in the future)
//! Example: current_time + 5 hours returns -> "this evening"
//! Note: This function was provided to provide daypart message for today and tomorrow
//! and provides a single phrase for the day after as well as a catchall beyond that
//! @param current_timestamp timestamp for the current time
//! @param hours_in_the_future hours after current_timestamp used to select the daypart message
//! @return const text containing daypart phrase
const char *clock_get_relative_daypart_string(time_t current_timestamp,
                                              uint32_t hours_in_the_future);

//! Adds minutes to wall clock time, wrapping around 24 hours.
void clock_hour_and_minute_add(int *hour, int *minute, int delta_minutes);

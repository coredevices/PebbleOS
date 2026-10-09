/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <time.h>

/**
 * @defgroup services_clock Clock
 * @ingroup services
 * @brief Wall clock time, timezone and user-facing time formatting.
 *
 * Keeps the RTC time and timezone, handles the time endpoint messages from the phone, tracks DST
 * transitions and formats times and dates according to the user's 12h/24h preference and
 * language. Entities shared with the app SDK are documented in the SDK's Wall Time group.
 */

/**
 * @addtogroup Foundation
 * @{
 */

/**
 * @addtogroup WallTime Wall Time
 * @brief Functions, data structures and other things related to wall clock time.
 *
 * This module contains utilities to get the current time and create strings with formatted
 * dates and times.
 * @{
 */

/** The maximum length for a timezone full name (e.g. America/Chicago) */
#define TIMEZONE_NAME_LENGTH 32
/**
 * @ingroup services_clock
 * @brief Buffer size for common strings like "Wednesday" or "30 minutes ago".
 */
#define TIME_STRING_REQUIRED_LENGTH 20
/**
 * @ingroup services_clock
 * @brief Buffer size for a time, e.g. "14:20".
 */
#define TIME_STRING_TIME_LENGTH 10
/**
 * @ingroup services_clock
 * @brief Buffer size for a day and month, e.g. "04/27".
 */
#define TIME_STRING_DATE_LENGTH 10
/**
 * @ingroup services_clock
 * @brief Buffer size for a day of the month, e.g. "27".
 */
#define TIME_STRING_DAY_DATE_LENGTH 3

/** @brief Weekday values */
typedef enum {
  /** Today */
  TODAY = 0,
  /** Sunday */
  SUNDAY,
  /** Monday */
  MONDAY,
  /** Tuesday */
  TUESDAY,
  /** Wednesday */
  WEDNESDAY,
  /** Thursday */
  THURSDAY,
  /** Friday */
  FRIDAY,
  /** Saturday */
  SATURDAY,
} WeekDay;

/**
 * @ingroup services_clock
 * @brief Initialize the clock service.
 *
 * Moves an invalid RTC time forward to the minimum valid boot timestamp, loads the timezone and
 * starts the per-minute DST watch.
 */
void clock_init(void);

#ifndef CONFIG_RECOVERY_FW
/**
 * @ingroup services_clock
 * @brief Arm the hourly chime.
 *
 * Call once the services it uses (system resources, alerts preferences, vibe pattern service) are
 * initialized.
 */
void clock_hourly_chime_arm(void);
#endif

/**
 * @ingroup services_clock
 * @brief Get the current local time from the RTC.
 *
 * @param[out] time_tm Current local time.
 */
void clock_get_time_tm(struct tm *time_tm);

/**
 * @ingroup services_clock
 * @brief Format a time of day according to the user's 12h/24h preference.
 *
 * In 12h style, "AM" or "PM" is appended.
 *
 * @param[out] buffer Output buffer.
 * @param size Size of @p buffer.
 * @param hours Hour, 0-23.
 * @param minutes Minute, 0-59.
 * @param add_space Whether to add a space between the time and AM/PM.
 * @return Length of the formatted string, as returned by snprintf(); 0 if @p buffer is NULL or
 *         @p size is 0.
 */
size_t clock_format_time(char *buffer, uint8_t size, int16_t hours, int16_t minutes,
                         bool add_space);

/**
 * @ingroup services_clock
 * @brief Same as clock_copy_time_string(), for a given timestamp.
 *
 * @param[out] buffer Output buffer.
 * @param size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string, as returned by snprintf().
 */
size_t clock_copy_time_string_timestamp(char *buffer, uint8_t size, time_t timestamp);

/**
 * @brief Copies a time string into the buffer, formatted according to the user's time display
 * preferences (such as 12h/24h time).
 *
 * Example results: "7:30" or "15:00".
 * @note AM/PM are also outputted with the time if the user's preference is 12h time.
 * @param[out] buffer A pointer to the buffer to copy the time string into
 * @param size The maximum size of buffer
 */
void clock_copy_time_string(char *buffer, uint8_t size);

/**
 * @ingroup services_clock
 * @brief Format a time as "7:30" or "15:00" depending on the user's 12h/24h preference.
 *
 * AM/PM is not included; see clock_get_time_word(). Leading whitespace is stripped.
 *
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string.
 */
size_t clock_get_time_number(char *buffer, size_t buffer_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get the AM/PM designator of a time.
 *
 * Use with clock_get_time_number() to form a full time.
 *
 * @param[out] buffer Output buffer; set to an empty string in 24h style.
 * @param buffer_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string, 0 in 24h style.
 */
size_t clock_get_time_word(char *buffer, size_t buffer_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get the relative time string of an event, split in a number and a word.
 *
 * E.g. "10" and " MIN. TO", so they can be rendered in different fonts. Close to the event, the
 * word is "Now" and the number empty; further away, the event time is used. All-day events and
 * middle days of multi-day events give "Today" or "All day".
 *
 * @param[out] number_buffer Output buffer for the number.
 * @param number_buffer_size Size of @p number_buffer.
 * @param[out] word_buffer Output buffer for the word.
 * @param word_buffer_size Size of @p word_buffer.
 * @param timestamp Event start time.
 * @param duration Event duration, in minutes.
 * @param current_day Midnight of the day being displayed.
 * @param all_day Whether the event lasts all day.
 */
void clock_get_event_relative_time_string(char *number_buffer, int number_buffer_size,
                                          char *word_buffer, int word_buffer_size, time_t timestamp,
                                          uint16_t duration, time_t current_day, bool all_day);

/**
 * @brief Gets the user's 12/24h clock style preference.
 * @return `true` if the user prefers 24h-style time display or `false` if the
 * user prefers 12h-style time display.
 */
bool clock_is_24h_style(void);

/**
 * @ingroup services_clock
 * @brief Set the user's time display style.
 *
 * @param is_24h_style true for 24h style, false for 12h style.
 */
void clock_set_24h_style(bool is_24h_style);

/**
 * @brief Checks if timezone is currently set, otherwise gmtime == localtime.
 * @return `true` if timezone has been set, false otherwise
 */
bool clock_is_timezone_set(void);

/**
 * @ingroup services_clock
 * @brief Check whether the timezone is selected manually.
 *
 * With a manual source the user selects the timezone in settings; otherwise the phone sets it.
 *
 * @return true if the timezone source is manual, false if it is the phone.
 */
bool clock_timezone_source_is_manual(void);

/**
 * @ingroup services_clock
 * @brief Set the timezone source.
 *
 * @param manual true to use a timezone selected in settings, false to use the phone's timezone.
 */
void clock_set_manual_timezone_source(bool manual);

/**
 * @ingroup services_clock
 * @brief Check whether the time is set manually.
 *
 * With a manual source the user sets the time in settings; otherwise the phone sets it.
 *
 * @return true if the time source is manual, false if it is the phone.
 */
bool clock_time_source_is_manual(void);

/**
 * @ingroup services_clock
 * @brief Set the time source.
 *
 * @param manual true to set the time on the watch, false to use the phone's time.
 */
void clock_set_manual_time_source(bool manual);

/**
 * @ingroup services_clock
 * @brief Ask the phone to send its current time.
 *
 * The phone answers with a set UTC and timezone message (sub-command 0x03). Does nothing if there
 * is no system session.
 */
void clock_request_time_from_phone(void);

/**
 * @ingroup services_clock
 * @brief Get the name of the current timezone region, e.g. "America/Chicago".
 *
 * Writes "---" if no timezone is set, or the UTC offset (e.g. "UTC-4") if the region is unknown.
 *
 * @param[out] region_name Output buffer, at least @ref TIMEZONE_NAME_LENGTH bytes.
 * @param buffer_size Size of @p region_name.
 */
void clock_get_timezone_region(char *region_name, const size_t buffer_size);

/**
 * @ingroup services_clock
 * @brief Get the current timezone region.
 *
 * @return Index of the current timezone in the timezone database.
 */
int16_t clock_get_timezone_region_id(void);

/**
 * @ingroup services_clock
 * @brief Switch to a timezone region and fire a time change event.
 *
 * @param region_id Index of the timezone in the timezone database.
 */
void clock_set_timezone_by_region_id(uint16_t region_id);

/**
 * @ingroup services_clock
 * @brief Set the current UTC time and fire a time change event.
 *
 * @param utc_time New UTC time.
 */
void clock_set_time(time_t utc_time);

/**
 * @brief Converts a (day, hour, minute) specification to a UTC timestamp occurring in the future.
 *
 * Always returns a timestamp for the next occurring instance,
 * example: specifying TODAY@14:30 when it is 14:40 will return a timestamp for tomorrow at
 * 14:30, while specifying the current day of the week will return a timestamp for 7 days from
 * now.
 * @param day WeekDay day of week including support for specifying TODAY
 * @param hour hour specified in 24-hour format [0-23]
 * @param minute minute [0-59]
 * @return UTC timestamp of the next occurrence.
 */
time_t clock_to_timestamp(WeekDay day, int hour, int minute);

/**
 * @ingroup services_clock
 * @brief Get a friendly date for a timestamp.
 *
 * "Today", "Yesterday" or "Tomorrow", the weekday name up to 5 days ahead, else e.g. "June 21".
 *
 * @param[out] buffer Output buffer.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to describe.
 */
void clock_get_friendly_date(char *buffer, int buf_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get a friendly time elapsed since a timestamp, e.g. "Now" or "5 minutes ago".
 *
 * Future timestamps are treated as now. Beyond 24 hours, or on another day, the date and time
 * are used.
 *
 * @param[out] buffer Output buffer.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to describe.
 */
void clock_get_since_time(char *buffer, int buf_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get a friendly time relative to a timestamp, e.g. "Now" or "In 5 hours".
 *
 * Past timestamps give "... ago". Beyond @p max_relative_hrs hours, or on another day, the date
 * and time are used.
 *
 * @param[out] buffer Output buffer.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to describe.
 * @param max_relative_hrs Number of hours for which a relative time is used.
 */
void clock_get_until_time(char *buffer, int buf_size, time_t timestamp, int max_relative_hrs);

/**
 * @ingroup services_clock
 * @brief clock_get_until_time_capitalized() that never writes the time of day.
 *
 * Where the full form would include a time, only the day is written (e.g. "Yesterday", "Monday").
 *
 * @param[out] buffer Output buffer.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to describe.
 * @param max_relative_hrs Number of hours for which a relative time is used.
 */
void clock_get_until_time_without_fulltime(char *buffer, int buf_size, time_t timestamp,
                                           int max_relative_hrs);

/**
 * @ingroup services_clock
 * @brief Get the date in MM/DD format.
 *
 * @param[out] buffer Output buffer, at least @ref TIME_STRING_DATE_LENGTH bytes.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string, as returned by strftime().
 */
size_t clock_get_date(char *buffer, int buf_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Same as clock_get_date(), from a broken-down time.
 *
 * Avoids a localtime_r() round trip in tick handlers, which already get a @c struct @c tm.
 *
 * @param[out] buffer Output buffer, at least @ref TIME_STRING_DATE_LENGTH bytes.
 * @param buf_size Size of @p buffer.
 * @param time_tm Local time to format.
 * @return Length of the formatted string, as returned by strftime().
 */
size_t clock_get_date_tm(char *buffer, int buf_size, const struct tm *time_tm);

/**
 * @ingroup services_clock
 * @brief Get the day of the month in DD format.
 *
 * @param[out] buffer Output buffer, at least @ref TIME_STRING_DAY_DATE_LENGTH bytes.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string, as returned by strftime().
 */
size_t clock_get_day_date(char *buffer, int buf_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get the date as month name and day, e.g. "July 16".
 *
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string.
 */
size_t clock_get_month_named_date(char *buffer, size_t buffer_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Get the date as abbreviated month name and day, e.g. "Jul 16".
 *
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer.
 * @param timestamp Time to format.
 * @return Length of the formatted string.
 */
size_t clock_get_month_named_abbrev_date(char *buffer, size_t buffer_size, time_t timestamp);

/**
 * @ingroup services_clock
 * @brief Capitalized clock_get_until_time(), e.g. "NOW" or "IN 5 H".
 *
 * @param[out] buffer Output buffer.
 * @param buf_size Size of @p buffer.
 * @param timestamp Time to describe.
 * @param max_relative_hrs Number of hours for which a relative time is used.
 */
void clock_get_until_time_capitalized(char *buffer, int buf_size, time_t timestamp,
                                      int max_relative_hrs);

/** @} */

/** @} */

/**
 * @addtogroup services_clock
 * @{
 */

/**
 * @brief Get a daypart phrase for a time in the future, e.g. "this evening".
 *
 * Covers today and tomorrow, then a single phrase for the day after and a catch-all beyond. The
 * phrase is a lower bound, as in "Powered 'til at least ...".
 *
 * @param current_timestamp Current time.
 * @param hours_in_the_future Hours after @p current_timestamp.
 * @return Untranslated phrase, to be passed through i18n.
 */
const char *clock_get_relative_daypart_string(time_t current_timestamp,
                                              uint32_t hours_in_the_future);

/**
 * @brief Add minutes to a wall clock time, wrapping around 24 hours.
 *
 * @param[in,out] hour Hour, 0-23.
 * @param[in,out] minute Minute, 0-59.
 * @param delta_minutes Minutes to add, may be negative.
 */
void clock_hour_and_minute_add(int *hour, int *minute, int delta_minutes);

/** @} */

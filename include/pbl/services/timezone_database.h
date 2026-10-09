/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/services/time.h>

/**
 * @defgroup services_timezone_database Timezone database
 * @ingroup services
 * @brief Read access to the timezone database stored in resources.
 *
 * The database holds the regions (continent/city name, UTC offset, abbreviation and DST rule
 * index), pairs of DST rules, and links mapping legacy region names to regions.
 * @{
 */

/** @brief Flags of a TimezoneDSTRule. */
typedef enum {
  /** Search backwards, rather than forwards, from TimezoneDSTRule::mday for the weekday. */
  TIMEZONE_FLAG_DAY_DECREMENT = 1 << 0,
  /** The transition time is in standard time rather than wall-clock time. */
  TIMEZONE_FLAG_STANDARD_TIME = 1 << 1,
  /** The transition time is in UTC rather than wall-clock time. */
  TIMEZONE_FLAG_UTC_TIME = 1 << 2,
} DSTRuleFlags;

/**
 * @brief Transition between standard time and DST.
 *
 * Matches the storage format exactly; do not change it without changing the database format.
 */
typedef struct {
  /**
   * @c 'D' when entering DST, @c 'S' when entering standard time, or a NUL character if the
   * timezone does not observe DST.
   */
  char ds_label;
  /** Day of the week, 0 being Sunday, or 255 for any day. */
  uint8_t wday;
  /** DSTRuleFlags bits. */
  uint8_t flag;
  /** Month, 0 being January. */
  uint8_t month;
  /** Day of the month, starting at 1. */
  uint8_t mday;
  /** Hour of the day; values of 24 and above roll over to the following days. */
  uint8_t hour;
  /** Minute of the hour. */
  uint8_t minute;

  /** @cond INTERNAL_HIDDEN */
  uint8_t padding;
  /** @endcond */
} TimezoneDSTRule;

/**
 * @brief Get the number of regions in the database.
 *
 * @return Number of regions.
 */
int timezone_database_get_region_count(void);

/**
 * @brief Load the timezone information of a region.
 *
 * TimezoneInfo::dst_start and TimezoneInfo::dst_end are set to 0; the DST period is not computed.
 *
 * @param region_id Region to look up.
 * @param[out] tz_info Timezone information.
 * @return True on success, false if the database could not be read.
 */
bool timezone_database_load_region_info(uint16_t region_id, TimezoneInfo *tz_info);

/**
 * @brief Load the name of a region.
 *
 * @param region_id Region to look up.
 * @param[out] region_name NUL terminated "continent/city" name, in a buffer of at least
 *             @c TIMEZONE_NAME_LENGTH bytes.
 * @return True on success, false if @p region_id is invalid.
 */
bool timezone_database_load_region_name(uint16_t region_id, char *region_name);

/**
 * @brief Load the pair of DST rules of a DST id.
 *
 * @param dst_id DST rule index, starting at 1.
 * @param[out] start Rule entering DST.
 * @param[out] end Rule leaving DST.
 * @return True on success, false if @p dst_id is invalid, the timezone does not observe DST or
 *         the database is malformed.
 */
bool timezone_database_load_dst_rule(uint8_t dst_id, TimezoneDSTRule *start, TimezoneDSTRule *end);

/**
 * @brief Find a region by name.
 *
 * Region names are matched on their first @p region_name_length characters. Links are searched
 * next, as phones may send legacy names such as "US/Pacific".
 *
 * @param region_name Name to look up, need not be NUL terminated.
 * @param region_name_length Length of @p region_name.
 * @return Region id, or -1 if not found.
 */
int timezone_database_find_region_by_name(const char *region_name, int region_name_length);

/** @} */

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/weather/weather_service.h"
#include "pbl/services/weather/weather_types.h"
#include "system/status_codes.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/pstring.h"
#include <time.h>
#include "pbl/util/uuid.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_blob_db_weather_db Weather database
 * @ingroup services_blob_db
 * @brief Weather locations (::BlobDBIdWeather), keyed by location UUID.
 *
 * Two record schemas coexist:
 * - v3, the legacy schema (current conditions, today and tomorrow), see ::WeatherDBEntryV3.
 * - v4, the rich schema for the full Weather app, see ::WeatherDBEntry. It appends today's
 *   extended metrics, the location coordinates, a multi-day daily forecast and today's hourly
 *   series after the v3 prefix, whose offsets are preserved, and before the trailing
 *   variable-length pstring16s. The version byte is the single source of compatibility.
 *
 * The phone only writes v4 records when the firmware advertises @c weather_db_v4_support;
 * otherwise it keeps writing v3. The firmware parses both.
 *
 * v4 minor versions append fixed fields before the trailing strings:
 * - 0: base v4.
 * - 1: @c location_utc_offset_min and @c daily_metrics.
 * - 2: today's raw warning readings (WMO code, humidity, minimum visibility, precipitation sum)
 *   for the weather report's warning line, and @c daily_feels_like.
 * - 3: dominant wind direction, today and per day.
 * - 4: today's hourly UV.
 * - 5: tomorrow's hourly type and temperature series.
 *
 * Older minors remain parseable: readers gate the appended fields on @c minor_version and the
 * record length, and the trailing strings offset is resolved per minor (see
 * weather_db_entry_get_strings()). Unknown future minors are rejected on insert since their
 * strings offset is unknowable, so the phone must gate each new minor on firmware support.
 * @{
 */

/** @brief Current major version of the record schema. */
#define WEATHER_DB_CURRENT_VERSION (4)
/** @brief Newest minor version of the v4 schema understood by the firmware. */
#define WEATHER_DB_CURRENT_MINOR_VERSION (5)
/** @brief Major version of the legacy record schema. */
#define WEATHER_DB_LEGACY_VERSION (3)

/** @brief Days of daily forecast in a v4 record (today + 6). */
#define WEATHER_DB_MAX_FORECAST_DAYS (7)
/** @brief Hours of hourly data in a v4 series (one day, keeps the record small). */
#define WEATHER_DB_HOURLY_COUNT (24)

/** @brief Record key, the location UUID. */
typedef Uuid WeatherDBKey;

/**
 * @brief Legacy v3 record.
 *
 * Kept verbatim so records written by older phone apps can still be read. Do not change.
 */
typedef struct PBL_PACKED {
  /** Schema version, ::WEATHER_DB_LEGACY_VERSION. */
  uint8_t version;
  /** Current temperature. */
  int16_t current_temp;
  /** Current conditions. */
  WeatherType current_weather_type;
  /** Today's high temperature. */
  int16_t today_high_temp;
  /** Today's low temperature. */
  int16_t today_low_temp;
  /** Tomorrow's conditions. */
  WeatherType tomorrow_weather_type;
  /** Tomorrow's high temperature. */
  int16_t tomorrow_high_temp;
  /** Tomorrow's low temperature. */
  int16_t tomorrow_low_temp;
  /** Time of the last update, UTC. */
  time_t last_update_time_utc;
  /** Whether this is the phone's current location. */
  bool is_current_location;
  /** Location name and short phrase, see ::WeatherDbStringIndex. */
  struct pbl_serialized_array pstring16s;
} WeatherDBEntryV3;

/** @brief One day of daily forecast (v4). */
typedef struct PBL_PACKED {
  /** High temperature, @c WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP if unknown. */
  int16_t high_temp;
  /** Low temperature, @c WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP if unknown. */
  int16_t low_temp;
  /** @c WeatherType stored as a byte, cast on read; 255 if unknown. */
  uint8_t weather_type;
} WeatherDBDailyForecast;

/**
 * @brief Extended metrics of one day (v4 minor 1), parallel to @c daily (index 0 is today).
 *
 * Shown on the scrolled forecast and when paging through days. 255 means unknown.
 */
typedef struct PBL_PACKED {
  /** Precipitation probability, 0 to 100 %. */
  uint8_t precip_probability;
  /** Wind speed in whole units, same unit as @c today_wind_speed. */
  uint8_t wind_speed;
  /** UV index times 10, 0 to 110. */
  uint8_t uv_index_x10;
} WeatherDBDailyMetrics;

/**
 * @brief v4 record.
 *
 * Layout: the v3 fixed prefix with unchanged offsets, the v4 fixed fields, then the trailing
 * pstring16s, which must stay last. Fields of a minor are only present when @c minor_version
 * is at least that minor; use weather_db_entry_get_strings() to locate the strings.
 */
typedef struct PBL_PACKED {
  /** Schema version, ::WEATHER_DB_CURRENT_VERSION. */
  uint8_t version;
  /** Current temperature. */
  int16_t current_temp;
  /** Current conditions. */
  WeatherType current_weather_type;
  /** Today's high temperature. */
  int16_t today_high_temp;
  /** Today's low temperature. */
  int16_t today_low_temp;
  /** Tomorrow's conditions. */
  WeatherType tomorrow_weather_type;
  /** Tomorrow's high temperature. */
  int16_t tomorrow_high_temp;
  /** Tomorrow's low temperature. */
  int16_t tomorrow_low_temp;
  /** Time of the last update, UTC. */
  time_t last_update_time_utc;
  /** Whether this is the phone's current location. */
  bool is_current_location;

  /** Minor version of the v4 schema. */
  uint8_t minor_version;
  /**
   * Today's feels-like temperature, @c WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP if
   * unknown.
   */
  int16_t today_feels_like_temp;
  /** Today's UV index times 10 (0 to 110), -1 if unknown. */
  int16_t today_uv_index_x10;
  /** Today's precipitation probability (0 to 100 %), -1 if unknown. */
  int16_t today_precip_probability;
  /** Today's wind speed in whole units (km/h or mph, as chosen on the phone), 0 if unknown. */
  uint16_t today_wind_speed;
  /** Today's wind direction in degrees (0 to 359), 0xFFFF if unknown. */
  uint16_t today_wind_direction;
  /** Latitude times 100, for the globe; @c INT16_MIN if unknown. */
  int16_t latitude_e2;
  /** Longitude times 100, for the globe; @c INT16_MIN if unknown. */
  int16_t longitude_e2;
  /** Valid entries in @ref daily (0 to ::WEATHER_DB_MAX_FORECAST_DAYS). */
  uint8_t num_daily;
  /** Daily forecast, index 0 is today. */
  WeatherDBDailyForecast daily[WEATHER_DB_MAX_FORECAST_DAYS];
  /** Entries in today's hourly series, 0 or ::WEATHER_DB_HOURLY_COUNT. */
  uint8_t today_hourly_count;
  /** @c WeatherType of each hour of today, 0 to 23. */
  uint8_t today_hourly_weather_type[WEATHER_DB_HOURLY_COUNT];
  /** Temperature of each hour of today, 0 to 23. */
  int8_t today_hourly_temp[WEATHER_DB_HOURLY_COUNT];

  /**
   * Location timezone in minutes east of UTC (e.g. Tokyo +540, New York DST -240);
   * @c INT16_MIN if unknown. Minor 1.
   *
   * Lets the watch show the location's local sunset and hourly times for saved cities.
   */
  int16_t location_utc_offset_min;
  /** Per-day extended metrics, parallel to @ref daily. Minor 1. */
  WeatherDBDailyMetrics daily_metrics[WEATHER_DB_MAX_FORECAST_DAYS];

  /** Today's WMO weather code (Open-Meteo daily weather_code), 0xFF if unknown. Minor 2. */
  uint8_t today_wmo_code;
  /** Today's mean relative humidity (0 to 100 %), 0xFF if unknown. Minor 2. */
  uint8_t today_humidity_pct;
  /** Today's minimum visibility in meters (clamped to 65534), 0xFFFF if unknown. Minor 2. */
  uint16_t today_visibility_m;
  /** Today's total precipitation in whole mm (clamped to 65534), 0xFFFF if unknown. Minor 2. */
  uint16_t today_precip_sum_mm;
  /**
   * Per-day feels-like maximum, parallel to @ref daily;
   * @c WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP if unknown. Minor 2.
   */
  int16_t daily_feels_like[WEATHER_DB_MAX_FORECAST_DAYS];

  /** Today's dominant wind direction in degrees (0 to 359), -1 if unknown. Minor 3. */
  int16_t today_wind_dir_deg;
  /** Per-day dominant wind direction in degrees (0 to 359), -1 if unknown. Minor 3. */
  int16_t daily_wind_dir_deg[WEATHER_DB_MAX_FORECAST_DAYS];

  /**
   * UV index times 10 for each hour of today (UV 6.5 is 65), 255 if unknown. Minor 4.
   *
   * Gives the current hour's UV; @ref today_uv_index_x10 stays the day's figure.
   */
  uint8_t today_hourly_uv_x10[WEATHER_DB_HOURLY_COUNT];

  /**
   * Entries in tomorrow's hourly series, 0 or ::WEATHER_DB_HOURLY_COUNT. Minor 5.
   *
   * Tomorrow's series mirrors today's for the next location-local day, so the clock dial,
   * which shows the next 12 hours, has data past midnight.
   */
  uint8_t tomorrow_hourly_count;
  /** @c WeatherType of each hour of tomorrow, 255 if unknown. Minor 5. */
  uint8_t tomorrow_hourly_weather_type[WEATHER_DB_HOURLY_COUNT];
  /** Temperature of each hour of tomorrow, 0 to 23. Minor 5. */
  int8_t tomorrow_hourly_temp[WEATHER_DB_HOURLY_COUNT];

  /**
   * Location name and short phrase, see ::WeatherDbStringIndex. Must stay last.
   *
   * Only at this offset in current minor records; use weather_db_entry_get_strings().
   */
  struct pbl_serialized_array pstring16s;
} WeatherDBEntry;

/** @brief Index of the strings in a record's pstring16s. */
typedef enum WeatherDbStringIndex {
  /** Location name. */
  WeatherDbStringIndex_LocationName,
  /** Short description of the conditions. */
  WeatherDbStringIndex_ShortPhrase,
  /** Number of strings. */
  WeatherDbStringIndexCount,
} WeatherDbStringIndex;

/**
 * @brief Fixed size of a v4.0 record, the minimum of any v4 record and where its strings start.
 */
#define WEATHER_DB_V4_0_FIXED_SIZE (offsetof(WeatherDBEntry, location_utc_offset_min))
/** @brief Fixed size of a v4.1 record, where its strings start. */
#define WEATHER_DB_V4_1_FIXED_SIZE (offsetof(WeatherDBEntry, today_wmo_code))
/** @brief Fixed size of a v4.2 record, where its strings start. */
#define WEATHER_DB_V4_2_FIXED_SIZE (offsetof(WeatherDBEntry, today_wind_dir_deg))
/** @brief Fixed size of a v4.3 record, where its strings start. */
#define WEATHER_DB_V4_3_FIXED_SIZE (offsetof(WeatherDBEntry, today_hourly_uv_x10))
/** @brief Fixed size of a v4.4 record, where its strings start. */
#define WEATHER_DB_V4_4_FIXED_SIZE (offsetof(WeatherDBEntry, tomorrow_hourly_count))
/** @brief Fixed size of a current (v4.5) record, everything but the trailing strings. */
#define WEATHER_DB_V4_FIXED_SIZE (offsetof(WeatherDBEntry, pstring16s))

/** @brief Smallest acceptable record, a legacy v3 record. */
#define MIN_ENTRY_SIZE (sizeof(WeatherDBEntryV3))
/** @brief Largest acceptable record. */
#define MAX_ENTRY_SIZE                                                         \
  (sizeof(WeatherDBEntry) + WEATHER_SERVICE_MAX_WEATHER_LOCATION_BUFFER_SIZE + \
   WEATHER_SERVICE_MAX_SHORT_PHRASE_BUFFER_SIZE)

/**
 * @brief Check whether the firmware can parse a major version.
 *
 * @param version Major version.
 * @return true for ::WEATHER_DB_CURRENT_VERSION and ::WEATHER_DB_LEGACY_VERSION.
 */
static inline bool weather_db_version_is_supported(uint8_t version) {
  return (version == WEATHER_DB_CURRENT_VERSION) || (version == WEATHER_DB_LEGACY_VERSION);
}

/**
 * @brief Check whether the firmware can parse a record.
 *
 * A v4 record with a minor newer than ::WEATHER_DB_CURRENT_MINOR_VERSION is rejected: a newer
 * minor appends fixed fields, which moves the trailing strings.
 *
 * @param entry Record.
 * @return true if supported.
 */
static inline bool weather_db_entry_is_supported(const WeatherDBEntry *entry) {
  if (!weather_db_version_is_supported(entry->version)) {
    return false;
  }
  return (entry->version < WEATHER_DB_CURRENT_VERSION) ||
         (entry->minor_version <= WEATHER_DB_CURRENT_MINOR_VERSION);
}

/**
 * @brief Get the offset of the trailing strings for a record version.
 *
 * Each minor places them differently: at the first field the next minor appends.
 *
 * @param version Major version.
 * @param minor_version Minor version, ignored for v3.
 * @return Byte offset of the pstring16s array.
 */
static inline size_t weather_db_entry_strings_offset(uint8_t version, uint8_t minor_version) {
  if (version < WEATHER_DB_CURRENT_VERSION) {
    return offsetof(WeatherDBEntryV3, pstring16s);
  }
  if (minor_version >= 5)
    return offsetof(WeatherDBEntry, pstring16s);
  if (minor_version >= 4)
    return WEATHER_DB_V4_4_FIXED_SIZE;
  if (minor_version >= 3)
    return WEATHER_DB_V4_3_FIXED_SIZE;
  if (minor_version >= 2)
    return WEATHER_DB_V4_2_FIXED_SIZE;
  if (minor_version >= 1)
    return WEATHER_DB_V4_1_FIXED_SIZE;
  return WEATHER_DB_V4_0_FIXED_SIZE;
}

/**
 * @brief Locate the trailing strings of a record.
 *
 * Use this instead of @c &entry->pstring16s, which is only valid for current minor records.
 *
 * @param entry Record of any supported version.
 * @return Pointer to the pstring16s array.
 */
static inline struct pbl_serialized_array *weather_db_entry_get_strings(WeatherDBEntry *entry) {
  const uint8_t minor = (entry->version >= WEATHER_DB_CURRENT_VERSION) ? entry->minor_version : 0;
  return (struct pbl_serialized_array *)((uint8_t *)entry +
                                         weather_db_entry_strings_offset(entry->version, minor));
}

/**
 * @brief Callback of weather_db_for_each().
 *
 * @param key Location UUID; only valid during the call.
 * @param entry Record, possibly a v3 record; only valid during the call.
 * @param context User data.
 */
typedef void (*WeatherDBIteratorCallback)(WeatherDBKey *key, WeatherDBEntry *entry, void *context);

/**
 * @brief Call a function for every supported record.
 *
 * The database is locked during the iteration.
 *
 * @param cb Callback.
 * @param context User data passed to @p cb.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_for_each(WeatherDBIteratorCallback cb, void *context);

/** @brief Initialize the weather database. */
void weather_db_init(void);

/**
 * @brief Delete all records of the weather database.
 *
 * Returns @c E_RANGE when the phone does not support the weather service, so it stops sending
 * records.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_flush(void);

/**
 * @brief Compact the settings file backing the weather database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_compact(void);

/**
 * @brief Insert or replace a record in the weather database.
 *
 * The key is the location UUID and the value a v3 or v4 record, bounds-checked against its
 * version. Returns @c E_RANGE when the phone does not support the weather service, so it stops
 * sending records.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the weather database.
 *
 * The key is the location UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int weather_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the weather database.
 *
 * The key is the location UUID. Unsupported records are deleted and reported as
 * @c E_DOES_NOT_EXIST.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the weather database.
 *
 * The key is the location UUID. Returns @c E_RANGE when the phone does not support the weather
 * service, so it stops sending records.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t weather_db_delete(const uint8_t *key, int key_len);

/** @} */

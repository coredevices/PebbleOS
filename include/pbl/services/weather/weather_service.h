/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/blob_db/weather_db.h>
#include <pbl/services/weather/weather_types.h>
#include <pbl/util/list.h>
#include <time.h>

#include <stdint.h>

/**
 * @defgroup services_weather Weather
 * @ingroup services
 * @brief Weather forecasts sent by the phone.
 *
 * Forecast and location data is pushed from the phone into the weather database; the watch never
 * requests it. Locations are ordered by the weather app preferences, the first one being the
 * default location. Entries last updated before yesterday are ignored. Subscribe to
 * @c PEBBLE_WEATHER_CHANGED_EVENT to learn about database changes.
 *
 * @code{.c}
 * WeatherLocationForecast *forecast = weather_service_create_default_forecast();
 * if (forecast) {
 *   PBL_LOG_DBG("%d degrees", forecast->current_temp);
 *   weather_service_destroy_default_forecast(forecast);
 * }
 * @endcode
 * @{
 */

/** @brief Buffer size for a short weather phrase. */
#define WEATHER_SERVICE_MAX_SHORT_PHRASE_BUFFER_SIZE (32)
/** @brief Buffer size for a location name. */
#define WEATHER_SERVICE_MAX_WEATHER_LOCATION_BUFFER_SIZE (64)
/** @brief Last update time marking an entry without valid data. */
#define WEATHER_SERVICE_INVALID_DATA_LAST_UPDATE_TIME (0)
/** @brief Temperature value meaning unknown. */
#define WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP (INT16_MAX)

/** @brief Index of a weather location in the user's ordering. */
typedef int WeatherLocationID;

/** @brief Forecast for one location, as sent by the phone. */
typedef struct WeatherLocationForecast {
  /** Location name, owned by the forecast. */
  char *location_name;
  /** Whether this is the phone's current location. */
  bool is_current_location;
  /** Current temperature. */
  int current_temp;
  /** Today's high temperature. */
  int today_high;
  /** Today's low temperature. */
  int today_low;
  /** Current conditions. */
  WeatherType current_weather_type;
  /** Short description of the current conditions, owned by the forecast. */
  char *current_weather_phrase;
  /** Tomorrow's high temperature. */
  int tomorrow_high;
  /** Tomorrow's low temperature. */
  int tomorrow_low;
  /** Tomorrow's conditions. */
  WeatherType tomorrow_weather_type;
  /** Time the forecast was last updated, UTC. */
  time_t time_updated_utc;
} WeatherLocationForecast;

/** @brief List node holding the forecast of one location. */
typedef struct WeatherDataListNode {
  /** List linkage. */
  ListNode node;
  /** Position of the location in the user's ordering. */
  WeatherLocationID id;
  /** Forecast of the location. */
  WeatherLocationForecast forecast;
} WeatherDataListNode;

/**
 * @brief Initialize the weather service.
 *
 * Caches the default location's forecast and keeps it up to date on database changes.
 */
void weather_service_init(void);

/**
 * @brief Get a copy of the default location's forecast.
 *
 * @return Forecast allocated on the caller's task heap, to be freed with
 *         weather_service_destroy_default_forecast(), or NULL if none is available.
 */
WeatherLocationForecast *weather_service_create_default_forecast(void);

/**
 * @brief Free a forecast created by weather_service_create_default_forecast().
 *
 * @param forecast Forecast to free, may be NULL.
 */
void weather_service_destroy_default_forecast(WeatherLocationForecast *forecast);

/**
 * @brief Build a list of the forecasts of all valid locations.
 *
 * The list is sorted by @ref WeatherDataListNode::id. Locations missing from the user's ordering
 * or with invalid or outdated data are skipped.
 *
 * @param[out] count_out Number of locations in the list. Left untouched if the location ordering
 *                       is unavailable.
 * @return Head of the list, to be freed with weather_service_locations_list_destroy(). May be
 *         NULL.
 */
WeatherDataListNode *weather_service_locations_list_create(size_t *count_out);

/**
 * @brief Get the list node at an index.
 *
 * @param head Head of a list from weather_service_locations_list_create().
 * @param index Index of the node.
 * @return Node at @p index, or NULL if out of range.
 */
WeatherDataListNode *weather_service_locations_list_get_location_at_index(WeatherDataListNode *head,
                                                                          unsigned int index);

/**
 * @brief Free a list created by weather_service_locations_list_create().
 *
 * @param head Head of the list, may be NULL.
 */
void weather_service_locations_list_destroy(WeatherDataListNode *head);

/**
 * @brief Check whether the connected phone app supports weather.
 *
 * Always true on QEMU.
 *
 * @return true if the phone reported weather support.
 */
bool weather_service_supported_by_phone(void);

/** @} */

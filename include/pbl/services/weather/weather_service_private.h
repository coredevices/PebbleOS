/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_weather_weather_service_private Weather app preferences
 * @ingroup services_weather
 * @brief Location ordering stored in the watch app preferences database.
 * @{
 */

/** @brief Key of the weather app preferences in the watch app preferences database. */
#define PREF_KEY_WEATHER_APP "weatherApp"

/** @brief Serialized weather app preferences. */
typedef struct PBL_PACKED SerializedWeatherAppPrefs {
  /** Number of entries in @ref locations. */
  uint8_t num_locations;
  /** Weather database keys of the locations, in display order; the first is the default. */
  Uuid locations[];
} SerializedWeatherAppPrefs;

/** @} */

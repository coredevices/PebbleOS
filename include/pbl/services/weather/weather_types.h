/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/graphics/gtypes.h>
#include <resource/timeline_resource_ids.auto.h>

/**
 * @defgroup services_weather_weather_types Weather types
 * @ingroup services_weather
 * @brief Weather conditions and their presentation.
 *
 * Timestamps of hourly data are exactly on the hour, those of daily data at midnight.
 * @{
 */

// TODO (PBL-36438): use proper enum naming
/**
 * @brief Weather condition, as sent by the phone.
 *
 * Generated from @c weather_type_tuples.def, the only place where entries may be added. Values
 * are @c WeatherType_PartlyCloudy (0), @c WeatherType_CloudyDay, @c WeatherType_LightSnow,
 * @c WeatherType_LightRain, @c WeatherType_HeavyRain, @c WeatherType_HeavySnow,
 * @c WeatherType_Generic, @c WeatherType_Sun, @c WeatherType_RainAndSnow (8) and
 * @c WeatherType_Unknown (255).
 */
typedef enum {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id) \
  WeatherType_##id = numeric_id,
#include "weather_type_tuples.def"
} WeatherType;

/**
 * @brief Get the name of a weather type.
 *
 * @param weather_type Weather type.
 * @return Identifier of the type, e.g. "PartlyCloudy".
 */
const char *weather_type_get_name(WeatherType weather_type);

/**
 * @brief Get the background color for a weather type.
 *
 * @param weather_type Weather type.
 * @return Background color, GColorClear on black and white displays.
 */
GColor weather_type_get_bg_color(WeatherType weather_type);

/**
 * @brief Get the text color to use over the weather type's background.
 *
 * @param weather_type Weather type.
 * @return Text color.
 */
GColor weather_type_get_text_color(WeatherType weather_type);

/**
 * @brief Get the timeline icon for a weather type.
 *
 * @param weather_type Weather type.
 * @return Timeline resource ID of the icon.
 */
TimelineResourceId weather_type_get_timeline_resource_id(WeatherType weather_type);

/** @} */

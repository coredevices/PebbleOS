/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Weather Types
//!
//! This file contains all the types for Weather Locations and Weather Data.
//! The weather_timestamp_utcs of all hourly data is exactly on the hour.
//! The weather_timestamp_utcs of all daily data is at midnight of the day.

#include "applib/graphics/gtypes.h"
#include "resource/timeline_resource_ids.auto.h"

// Do NOT add entries here. See weather_type_tuples.def
// TODO (PBL-36438): use proper enum naming
typedef enum {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id)\
    WeatherType_##id = numeric_id,
#include "services/normal/weather/weather_type_tuples.def"
} WeatherType;

// ------------------------------------------------------------------------------------
const char *weather_type_get_name(WeatherType weather_type);

GColor weather_type_get_bg_color(WeatherType weather_type);

GColor weather_type_get_text_color(WeatherType weather_type);

TimelineResourceId weather_type_get_timeline_resource_id(WeatherType weather_type);

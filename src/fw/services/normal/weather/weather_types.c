/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource/timeline_resource_ids.auto.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/gcolor_definitions.h"
#include "services/normal/weather/weather_types.h"
#include "util/size.h"

// Do NOT add entries to the following arrays. See weather_type_tuples.def
static const char *s_weather_type_names[] = {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id) #id,
#include "services/normal/weather/weather_type_tuples.def"
};

#if PBL_COLOR
static uint8_t s_weather_type_bg_colors[] = {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id) bg_color,
#include "services/normal/weather/weather_type_tuples.def"
};
#endif

static uint8_t s_weather_type_text_colors[] = {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id) text_color,
#include "services/normal/weather/weather_type_tuples.def"
};

static TimelineResourceId s_weather_type_timeline_resource_ids[] = {
#define WEATHER_TYPE_TUPLE(id, numeric_id, bg_color, text_color, timeline_resource_id) \
    timeline_resource_id,
#include "services/normal/weather/weather_type_tuples.def"
};

static const size_t s_num_weather_types = ARRAY_LENGTH(s_weather_type_names);

static size_t prv_get_array_index_of_type(WeatherType type) {
  return (type == WeatherType_Unknown) ? (s_num_weather_types - 1) : type;
}

const char *weather_type_get_name(WeatherType weather_type) {
  return s_weather_type_names[prv_get_array_index_of_type(weather_type)];
};

GColor weather_type_get_bg_color(WeatherType weather_type) {
#if PBL_COLOR
  const size_t index = prv_get_array_index_of_type(weather_type);
#endif
  return PBL_IF_COLOR_ELSE((GColor) {.argb = s_weather_type_bg_colors[index]}, GColorClear);
};

GColor weather_type_get_text_color(WeatherType weather_type) {
  return (GColor) {.argb = s_weather_type_text_colors[prv_get_array_index_of_type(weather_type)]};
};

TimelineResourceId weather_type_get_timeline_resource_id(WeatherType weather_type) {
  return s_weather_type_timeline_resource_ids[prv_get_array_index_of_type(weather_type)];
};

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/weather/weather_types.h>

TimelineResourceId PBL_WEAK weather_type_get_timeline_resource_id(WeatherType weather_type) {
  return 0;
}

const char *PBL_WEAK weather_type_get_phrase(WeatherType weather_type) {
  return nullptr;
}

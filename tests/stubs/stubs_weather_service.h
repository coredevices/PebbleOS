/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/weather/weather_service.h"
#include "util/attributes.h"

WeatherLocationForecast * WEAK weather_service_create_default_forecast(void) {
  return NULL;
}


void WEAK weather_service_destroy_default_forecast(WeatherLocationForecast *forecast) {}

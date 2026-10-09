/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/weather/weather_service.h>

WeatherLocationForecast *PBL_WEAK weather_service_create_default_forecast(void) {
  return nullptr;
}

void PBL_WEAK weather_service_destroy_default_forecast(WeatherLocationForecast *forecast) {
}

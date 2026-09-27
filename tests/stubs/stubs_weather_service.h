/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/weather/weather_service.h"
#include "pbl/kernel/compiler.h"

WeatherLocationForecast *PBL_WEAK weather_service_create_default_forecast(void) {
  return NULL;
}

void PBL_WEAK weather_service_destroy_default_forecast(WeatherLocationForecast *forecast) {
}

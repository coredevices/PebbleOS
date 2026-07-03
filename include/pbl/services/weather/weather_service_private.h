/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/attributes.h"
#include "pbl/util/uuid.h"

#define PREF_KEY_WEATHER_APP "weatherApp"

typedef struct PACKED SerializedWeatherAppPrefs {
  uint8_t num_locations;
  Uuid locations[];
} SerializedWeatherAppPrefs;

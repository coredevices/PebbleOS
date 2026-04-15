/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/uuid.h"

#define PREF_KEY_WEATHER_APP "weatherApp"

typedef struct PACKED SerializedWeatherAppPrefs {
  uint8_t num_locations;
  Uuid locations[];
} SerializedWeatherAppPrefs;

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/timeline_resources.h"
#include "pbl/kernel/compiler.h"

TimelineResourceId PBL_WEAK weather_type_get_timeline_resource_id(WeatherType weather_type) {
  return 0;
}

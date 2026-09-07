/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "metrics.h"

#include <stddef.h>
#include <stdint.h>

#include <pbl/services/activity/hr_util.h>

typedef struct WorkoutData {
  int32_t steps;
  int32_t duration_s;
  int32_t distance_m;
  int32_t avg_pace;
  int32_t bpm;
  int32_t hr_samples;
  int32_t active_kcal;
  int32_t avg_bpm;
  int32_t spo2;
  int32_t spo2_min;
  bool spo2_measuring;
  int32_t hr_zone_time_s[HRZoneCount];
  HRZone hr_zone;
} WorkoutData;

bool workout_data_update(void *workout_data);

void workout_data_fill_metric_value(WorkoutMetricType type, char *buffer, size_t buffer_size,
                                    void *i18n_owner, void *workout_data);

int32_t workout_data_get_metric_value(WorkoutMetricType type, void *workout_data);

int32_t workout_data_get_hr_samples(void *workout_data);

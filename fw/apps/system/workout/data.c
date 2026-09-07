/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "data.h"

#include <stdio.h>
#include <string.h>

#include <pbl/services/activity/health_util.h>
#include <pbl/services/activity/workout_service.h>
#include <pbl/util/units.h>

bool workout_data_update(void *data) {
  if (!data) {
    return false;
  }
  WorkoutData *workout_data = data;

  WorkoutServiceMetrics metrics;
  if (!workout_service_get_current_workout_metrics(&metrics)) {
    return false;
  }
  workout_data->steps = metrics.steps;
  workout_data->duration_s = metrics.duration_s;
  workout_data->distance_m = metrics.distance_m;
  workout_data->bpm = metrics.current_bpm;
  workout_data->hr_zone = metrics.current_hr_zone;
  workout_data->hr_samples = metrics.hr_samples;
  workout_data->active_kcal = metrics.active_kcal;
  workout_data->avg_bpm = metrics.avg_bpm;
  workout_data->spo2 = metrics.spo2_pct;
  workout_data->spo2_min = metrics.spo2_min_pct;
  workout_data->spo2_measuring = metrics.spo2_measuring;
  memcpy(workout_data->hr_zone_time_s, metrics.hr_zone_time_s, sizeof(metrics.hr_zone_time_s));

  workout_data->avg_pace = 0;
  if (workout_data->duration_s && workout_data->distance_m) {
    workout_data->avg_pace =
        health_util_get_pace(workout_data->duration_s, workout_data->distance_m);
  }
  return true;
}

// Metrics that have no value yet show a placeholder instead of a number.
static bool prv_metric_available(WorkoutMetricType type, WorkoutData *workout_data) {
  switch (type) {
    case WorkoutMetricType_Hr:
      return workout_data->bpm > 0;
    case WorkoutMetricType_AvgHr:
      return workout_data->avg_bpm > 0;
    case WorkoutMetricType_SpO2:
      return workout_data_get_metric_value(type, workout_data) > 0;
    case WorkoutMetricType_SpO2Min:
      return workout_data->spo2_min > 0;
    case WorkoutMetricType_AvgSpeed:
    case WorkoutMetricType_AvgCadence:
      return workout_data->duration_s > 0;
    case WorkoutMetricType_AvgPace:
      return workout_data->avg_pace > 0 && workout_data->avg_pace < PBL_SEC_PER_HOUR;
    default:
      return true;
  }
}

void workout_data_fill_metric_value(WorkoutMetricType type, char *buffer, size_t buffer_size,
                                    void *i18n_owner, void *data) {
  if (!prv_metric_available(type, data)) {
    snprintf(buffer, buffer_size, "--");
    return;
  }
  int32_t metric_value = workout_data_get_metric_value(type, data);

  switch (type) {
    case WorkoutMetricType_Hr:
    case WorkoutMetricType_AvgHr:
    case WorkoutMetricType_SpO2:
    case WorkoutMetricType_SpO2Min:
    case WorkoutMetricType_Steps:
    case WorkoutMetricType_AvgCadence:
    case WorkoutMetricType_ActiveCalories: {
      snprintf(buffer, buffer_size, "%" PRId32, metric_value);
      break;
    }
    case WorkoutMetricType_Distance:
    case WorkoutMetricType_AvgSpeed: {
      const int conversion_factor = health_util_get_distance_factor();
      health_util_format_whole_and_decimal(buffer, buffer_size, metric_value, conversion_factor);
      break;
    }
    case WorkoutMetricType_Duration: {
      health_util_format_hours_minutes_seconds(buffer, buffer_size, metric_value, true, i18n_owner);
      break;
    }
    case WorkoutMetricType_HrZone0Time:
    case WorkoutMetricType_HrZone1Time:
    case WorkoutMetricType_HrZone2Time:
    case WorkoutMetricType_HrZone3Time:
    case WorkoutMetricType_Pace:
    case WorkoutMetricType_AvgPace: {
      health_util_format_hours_minutes_seconds(buffer, buffer_size, metric_value, false,
                                               i18n_owner);
      break;
    }
    case WorkoutMetricType_Speed:
      // Not part of the workout service yet
    case WorkoutMetricType_Custom:
      // Sports app only
    case WorkoutMetricType_None:
    case WorkoutMetricTypeCount:
      break;
  }
}

int32_t workout_data_get_metric_value(WorkoutMetricType type, void *data) {
  WorkoutData *workout_data = data;

  switch (type) {
    case WorkoutMetricType_Hr:
      return workout_data->bpm;
    case WorkoutMetricType_AvgHr:
      return workout_data->avg_bpm;
    case WorkoutMetricType_SpO2:
      // -1 while a reading is in progress.
      return workout_data->spo2_measuring ? -1 : workout_data->spo2;
    case WorkoutMetricType_SpO2Min:
      return workout_data->spo2_min;
    case WorkoutMetricType_HrZone0Time:
    case WorkoutMetricType_HrZone1Time:
    case WorkoutMetricType_HrZone2Time:
    case WorkoutMetricType_HrZone3Time:
      return workout_data->hr_zone_time_s[type - WorkoutMetricType_HrZone0Time];
    case WorkoutMetricType_Duration:
      return workout_data->duration_s;
    case WorkoutMetricType_AvgPace:
      return workout_data->avg_pace;
    case WorkoutMetricType_Distance:
      return workout_data->distance_m;
    case WorkoutMetricType_Steps:
      return workout_data->steps;
    case WorkoutMetricType_ActiveCalories:
      return workout_data->active_kcal;
    case WorkoutMetricType_AvgSpeed:
      // Meters per hour; formatting applies the user's distance units.
      return workout_data->duration_s > 0
                 ? (int64_t)workout_data->distance_m * PBL_SEC_PER_HOUR / workout_data->duration_s
                 : 0;
    case WorkoutMetricType_AvgCadence:
      return workout_data->duration_s > 0
                 ? ((int64_t)workout_data->steps * PBL_SEC_PER_MIN + workout_data->duration_s / 2) /
                       workout_data->duration_s
                 : 0;
    default:
      return 0;
  }
}

int32_t workout_data_get_hr_samples(void *data) {
  WorkoutData *workout_data = data;
  return workout_data->hr_samples;
}

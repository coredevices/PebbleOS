/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "workout_data.h"

#include "services/normal/activity/health_util.h"
#include "services/normal/activity/workout_service.h"

#include <stdio.h>

void workout_data_update(void *data) {
  if (!data) {
    return;
  }
  WorkoutData *workout_data = data;

  workout_service_get_current_workout_info(&workout_data->steps, &workout_data->duration_s,
                                           &workout_data->distance_m, &workout_data->bpm,
                                           &workout_data->hr_zone);

  if (workout_data->duration_s && workout_data->distance_m) {
    workout_data->avg_pace = health_util_get_pace(workout_data->duration_s,
                                                  workout_data->distance_m);
  }
}

void workout_data_fill_metric_value(WorkoutMetricType type, char *buffer, size_t buffer_size,
                                    void *i18n_owner, void *data) {
  int32_t metric_value = workout_data_get_metric_value(type, data);

  switch (type) {
    case WorkoutMetricType_Hr:
    case WorkoutMetricType_Steps:
    {
      snprintf(buffer, buffer_size, "%"PRId32, metric_value);
      break;
    }
    case WorkoutMetricType_Distance:
    {
      const int conversion_factor = health_util_get_distance_factor();
      health_util_format_whole_and_decimal(buffer, buffer_size, metric_value, conversion_factor);
      break;
    }
    case WorkoutMetricType_Duration:
    {
      health_util_format_hours_minutes_seconds(buffer, buffer_size, metric_value,
                                               true, i18n_owner);
      break;
    }
    case WorkoutMetricType_Pace:
    case WorkoutMetricType_AvgPace:
    {
      health_util_format_hours_minutes_seconds(buffer, buffer_size, metric_value,
                                               false, i18n_owner);
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
    case WorkoutMetricType_Duration:
      return workout_data->duration_s;
    case WorkoutMetricType_AvgPace:
      return workout_data->avg_pace;
    case WorkoutMetricType_Distance:
      return workout_data->distance_m;
    case WorkoutMetricType_Steps:
      return workout_data->steps;
    default:
      return 0;
  }
}

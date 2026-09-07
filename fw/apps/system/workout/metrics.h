/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum {
  WorkoutMetricType_None = 0,
  WorkoutMetricType_Hr,
  WorkoutMetricType_Duration,
  WorkoutMetricType_Distance,
  WorkoutMetricType_Steps,
  WorkoutMetricType_Pace,
  WorkoutMetricType_AvgPace,
  WorkoutMetricType_Speed,
  WorkoutMetricType_Custom,
  WorkoutMetricType_AvgSpeed,
  WorkoutMetricType_AvgCadence,
  WorkoutMetricType_ActiveCalories,
  WorkoutMetricType_AvgHr,
  //! Percent, 0 before the first reading and -1 while one is in progress.
  WorkoutMetricType_SpO2,
  WorkoutMetricType_SpO2Min,
  WorkoutMetricType_HrZone0Time,
  WorkoutMetricType_HrZone1Time,
  WorkoutMetricType_HrZone2Time,
  WorkoutMetricType_HrZone3Time,
  WorkoutMetricTypeCount,
} WorkoutMetricType;

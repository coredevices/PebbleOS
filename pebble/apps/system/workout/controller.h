/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "metrics.h"

#include <stddef.h>
#include <stdint.h>

typedef struct WorkoutController {
  bool (*is_paused)(void);
  bool (*pause)(bool should_be_paused);
  bool (*stop)(void);

  //! Refresh the data; false once the workout is over, e.g. ended by another client.
  bool (*update_data)(void *data);
  void (*metric_to_string)(WorkoutMetricType type, char *buffer, size_t buffer_size,
                           void *i18n_owner, void *workout_data);
  int32_t (*get_metric_value)(WorkoutMetricType type, void *data);
  //! Heart rate readings so far, optional. The dashboard beats its heart on each new one.
  int32_t (*get_hr_samples)(void *data);
  const char *(*get_distance_string)(const char *miles_string, const char *km_string);
  char *(*get_custom_metric_label_string)(void);
} WorkoutController;

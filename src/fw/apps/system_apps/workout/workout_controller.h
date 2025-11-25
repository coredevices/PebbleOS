/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "workout_metrics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WorkoutController {
  bool (*is_paused)(void);
  bool (*pause)(bool should_be_paused);
  bool (*stop)(void);

  void (*update_data)(void *data);
  void (*metric_to_string)(WorkoutMetricType type, char *buffer,
                           size_t buffer_size, void *i18n_owner, void *workout_data);
  int32_t (*get_metric_value)(WorkoutMetricType type, void *data);
  const char* (*get_distance_string)(const char *miles_string, const char *km_string);
  char* (*get_custom_metric_label_string)(void);
} WorkoutController;

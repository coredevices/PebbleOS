/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "workout_controller.h"
#include "workout_metrics.h"

#include "services/normal/activity/activity.h"

typedef struct WorkoutActiveWindow WorkoutActiveWindow;


WorkoutActiveWindow *workout_active_create_single_layout(WorkoutMetricType metric,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller);

WorkoutActiveWindow *workout_active_create_double_layout(WorkoutMetricType top_metric,
                                                         int num_scrollable_metrics,
                                                         WorkoutMetricType *scrollable_metrics,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller);

WorkoutActiveWindow *workout_active_create_tripple_layout(WorkoutMetricType top_metric,
                                                          WorkoutMetricType middle_metric,
                                                          int num_scrollable_metrics,
                                                          WorkoutMetricType *scrollable_metrics,
                                                          void *workout_data,
                                                          WorkoutController *workout_controller);

WorkoutActiveWindow *workout_active_create_for_activity_type(ActivitySessionType type,
                                                             void *workout_data,
                                                             WorkoutController *workout_controller);

void workout_active_window_push(WorkoutActiveWindow *window);

void workout_active_update_scrollable_metrics(WorkoutActiveWindow *active_window,
                                              int num_scrollable_metrics,
                                              WorkoutMetricType *scrollable_metrics);

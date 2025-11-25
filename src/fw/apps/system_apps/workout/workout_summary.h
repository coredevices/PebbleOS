/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "workout_selection.h"

#include "services/normal/activity/activity.h"

typedef void (*StartWorkoutCallback)(ActivitySessionType type);

typedef struct WorkoutSummaryWindow WorkoutSummaryWindow;

WorkoutSummaryWindow *workout_summary_window_create(ActivitySessionType activity_type,
                                                    StartWorkoutCallback start_workout_cb,
                                                    SelectWorkoutCallback select_workout_cb);

void workout_summary_window_push(WorkoutSummaryWindow *window);

void workout_summary_update_activity_type(WorkoutSummaryWindow *summary_window,
                                          ActivitySessionType activity_type);

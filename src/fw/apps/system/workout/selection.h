/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/activity/activity.h"

typedef void (*SelectWorkoutCallback)(ActivitySessionType type);

typedef struct WorkoutSelectionWindow WorkoutSelectionWindow;

WorkoutSelectionWindow *workout_selection_push(SelectWorkoutCallback select_workout_cb);

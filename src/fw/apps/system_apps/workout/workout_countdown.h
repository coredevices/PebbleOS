/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "workout_summary.h"

#include "services/normal/activity/activity.h"

void workout_countdown_start(ActivitySessionType type, StartWorkoutCallback start_workout_cb);

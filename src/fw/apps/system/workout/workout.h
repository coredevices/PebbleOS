/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

typedef enum {
  WorkoutLaunchArg_EndWorkout = 1,
} WorkoutLaunchArg;

void workout_push_summary_window(void);

//! Call for system to obtain information about the application
//! @return System information about the app
const PebbleProcessMd *workout_app_get_info(void);

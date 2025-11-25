/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/activity/activity.h"
#include "services/normal/activity/hr_util.h"

#include <stdint.h>
#include <stdbool.h>

bool workout_service_is_workout_ongoing(void);

bool workout_service_start_workout(ActivitySessionType type);

bool workout_service_pause_workout(bool should_be_paused);

bool workout_service_stop_workout(void);

bool workout_service_is_paused(void);

bool workout_service_get_current_workout_type(ActivitySessionType *type_out);

bool workout_service_get_current_workout_info(int32_t *steps_out, int32_t *duration_s_out,
                                              int32_t *distance_m_out, int32_t *current_bpm_out,
                                              HRZone *current_hr_zone_out);

bool workout_service_set_current_workout_info(int32_t steps, int32_t duration_s,
                                              int32_t distance_m, int32_t current_bpm,
                                              HRZone current_hr_zone);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/activity/activity.h"

void workout_utils_send_abandoned_workout_notification(void);

const char* workout_utils_get_name_for_activity(ActivitySessionType type);

const char* workout_utils_get_detection_text_for_activity(ActivitySessionType type);

bool workout_utils_find_ongoing_activity_session(ActivitySession *session_out);

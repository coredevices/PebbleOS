/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/alarms/alarm.h"
#include "alarm_editor.h"

void alarm_detail_window_push(AlarmId alarm_id, AlarmInfo *alarm_info,
                              AlarmEditorCompleteCallback alarm_editor_callback,
                              void *callback_context);

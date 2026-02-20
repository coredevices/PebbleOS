/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/alarms/alarm.h"
#include "applib/ui/window.h"

typedef enum {
  CREATED,
  DELETED,
  EDITED,
  CANCELLED
} AlarmEditorResult;

typedef void (*AlarmEditorCompleteCallback)(AlarmEditorResult result, AlarmId id,
                                            void *callback_context);

Window* alarm_editor_create_new_alarm(AlarmEditorCompleteCallback editor_complete_callback,
                                      void *callback_context);

void alarm_editor_update_alarm_time(AlarmId alarm_id, AlarmType alarm_type,
                                    AlarmEditorCompleteCallback editor_complete_callback,
                                    void *callback_context);

void alarm_editor_update_alarm_days(AlarmId alarm_id,
                                    AlarmEditorCompleteCallback editor_complete_callback,
                                    void *callback_context);

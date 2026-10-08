/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/alarms/alarm_pin.h>

status_t alarm_pin_add(time_t alarm_time, AlarmId id, AlarmType type, AlarmKind kind,
                       Uuid *uuid_out) {
  return S_SUCCESS;
}

void alarm_pin_remove(Uuid *alarm_id) {
}

status_t alarm_pin_remove_untracked_future(time_t now, const Uuid *tracked, size_t tracked_count) {
  return S_SUCCESS;
}

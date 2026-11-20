/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/alarms/alarm_pin.h>

PBL_WEAK int alarm_pin_add(time_t alarm_time, AlarmId id, AlarmType type, AlarmKind kind,
                           Uuid *uuid_out) {
  return 0;
}

PBL_WEAK void alarm_pin_remove(Uuid *alarm_id) {
}

PBL_WEAK int alarm_pin_remove_untracked_future(time_t now, const Uuid *tracked,
                                               size_t tracked_count) {
  return 0;
}

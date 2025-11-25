/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"
#include "services/normal/alarms/alarm.h"

void alarm_pin_add(time_t alarm_time, AlarmId id, AlarmType type, AlarmKind kind, Uuid *uuid_out);

void alarm_pin_remove(Uuid *alarm_id);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "services/common/battery/battery_state.h"
#include "services/common/new_timer/new_timer.h"
#include <stdbool.h>

// The battery monitor handles power state and associated service control, in response to battery
// state changes. This includes low power and critical modes.

void battery_monitor_init(void);
void battery_monitor_handle_state_change_event(PreciseBatteryChargeState state);

// Use the battery state to determine if UI elements should be locked out
// because the battery is too low
bool battery_monitor_critical_lockout(void);

// For unit tests
TimerID battery_monitor_get_standby_timer_id(void);

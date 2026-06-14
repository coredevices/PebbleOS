/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef struct GAPLEConnection GAPLEConnection;

bool ble_hrm_is_supported_and_enabled(void);

bool ble_hrm_is_sharing_to_connection(const GAPLEConnection *connection);

bool ble_hrm_is_sharing(void);

void ble_hrm_handle_activity_prefs_heart_rate_is_enabled(bool is_enabled);

void ble_hrm_handle_disconnection(GAPLEConnection *connection);

//! Enable or disable BLE HRM workout mode.
//! When enabled, the HRM service is advertised and subscribed devices automatically receive HR data
//! for the duration of the workout. This is the only path through which the watch shares HR over
//! BLE; outside of an active workout (with the sharing setting enabled) no sharing occurs.
void ble_hrm_set_workout_mode(bool enabled);

void ble_hrm_init(void);

void ble_hrm_deinit(void);

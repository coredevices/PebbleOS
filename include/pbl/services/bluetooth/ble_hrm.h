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

void ble_hrm_init(void);

void ble_hrm_deinit(void);

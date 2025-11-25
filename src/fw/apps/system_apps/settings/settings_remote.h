/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "kernel/events.h"
#include "settings_bluetooth.h"

void settings_remote_menu_push(struct SettingsBluetoothData *bt_data, StoredRemote* stored_remote);

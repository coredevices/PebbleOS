/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/bluetooth/ble_client.h"

typedef enum {
  AppLaunchCharacteristicAppLaunch,
  AppLaunchCharacteristicNum
} AppLaunchCharacteristic;

void app_launch_handle_service_discovered(pbl_bt_characteristic_t *characteristics);

void app_launch_invalidate_all_references(void);

void app_launch_handle_service_removed(pbl_bt_characteristic_t *characteristics,
                                       uint8_t num_characteristics);

bool app_launch_can_handle_characteristic(pbl_bt_characteristic_t characteristic);

void app_launch_handle_disconnection(void);

void app_launch_trigger(void);

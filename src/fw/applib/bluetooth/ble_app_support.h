/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ble_scan.h"
#include "ble_central.h"
#include "ble_client.h"

#include "applib/event_service_client.h"

typedef struct {
  // Scanning
  EventServiceInfo scan_service_info;
  BLEScanHandler scan_handler;

  // Connecting
  EventServiceInfo connection_service_info;
  BLEConnectionHandler connection_handler;

  // GATT Client
  EventServiceInfo gatt_client_service_info;

  BLEClientServiceChangeHandler gatt_service_change_handler;

  BLEClientReadHandler gatt_characteristic_read_handler;
  BLEClientWriteHandler gatt_characteristic_write_handler;
  BLEClientSubscribeHandler gatt_characteristic_subscribe_handler;

  BLEClientReadDescriptorHandler gatt_descriptor_read_handler;
  BLEClientWriteDescriptorHandler gatt_descriptor_write_handler;

  uint8_t gatt_client_num_handlers;
} BLEAppState;

//! Initializes the static BLE state for the currently running app.
void ble_init_app_state(void);

//! Must be called by the kernel, after an app is killed to stop any ongoing
//! BLE activity that was running on behalf of the app.
void ble_app_cleanup(void);

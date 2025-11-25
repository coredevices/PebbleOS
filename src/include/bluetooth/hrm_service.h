/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <bluetooth/bluetooth_types.h>

typedef struct {
  uint16_t bpm;
  bool is_on_wrist;
} BleHrmServiceMeasurement;

//! @return True if the BT driver lib supports exposing the GATT HRM service.
bool bt_driver_is_hrm_service_supported(void);

//! Adds or removes the HRM service from the GATT database, notifying any connected devices
//! by sending a "Service Changed" indication for the mutated handle range.
void bt_driver_hrm_service_enable(bool enable);

//! Sends the Heart Rate Measurement to all subscribed & connected devices.
void bt_driver_hrm_service_handle_measurement(const BleHrmServiceMeasurement *measurement,
                                              const BTDeviceInternal *permitted_devices,
                                              size_t num_permitted_devices);

//! Called when a connected device (un)subscribes to the GATT HRM service's "Heart Rate Measurement"
//! characteristic.
extern void bt_driver_cb_hrm_service_update_subscription(const BTDeviceInternal *device,
                                                         bool is_subscribed);

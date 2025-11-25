/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gap_le_connection.h"

#include <bluetooth/bluetooth_types.h>

//! Requests the device name, caches the result in bt_persistent_storage and into
//! connection->device_name.
void gap_le_device_name_request(const BTDeviceInternal *address);

//! Convenience wrapper to request the device name for each connected BLE device, by calling
//! gap_le_device_name_request for each connection.
void gap_le_device_name_request_all(void);

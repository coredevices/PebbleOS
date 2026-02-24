/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>

//! @param address Pass NULL to disconnect the "active" remote.
void bt_driver_classic_disconnect(const BTDeviceAddress* address);

bool bt_driver_classic_is_connected(void);

bool bt_driver_classic_copy_connected_address(BTDeviceAddress* address);

bool bt_driver_classic_copy_connected_device_name(char name[BT_DEVICE_NAME_BUFFER_SIZE]);

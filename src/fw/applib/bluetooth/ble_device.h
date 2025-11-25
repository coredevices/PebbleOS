/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

//! Copies the devices that are known to the system. This set includes all
//! paired devices (connected or not) and devices for which there is a Bluetooth
//! connection to the system (but not necessarily paired and not necessarily
//! connected to the application).
//! @param[out] devices_out An array of BTDevice`s into which the known
//! devices will be copied.
//! @param[in,out] num_devices_out In: the size of the devices_out array.
//! Out: the number of BTDevice`s that were copied.
//! @return The total number of known devices. This might be a larger
//! number than num_devices_out will contain, if the passed array was not large
//! enough to hold all the connected devices.
uint8_t ble_device_copy_known_devices(BTDevice *devices_out,
                                      uint8_t *num_devices_out);

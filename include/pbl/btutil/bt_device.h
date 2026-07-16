/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>

//! Creates a BTDevice given its address.
//! @param address The address to use
//! @param is_random Specify true if the address is a random address, or false
//! if it is the real BD_ADDR of the device.
//! @return The created BTDevice
BTDevice bt_device_init_with_address(BTDeviceAddress address, bool is_random);

//! Gets the address of the device.
//! @param device The device for which to get the address.
//! @return The address of the device.
BTDeviceAddress bt_device_get_address(BTDevice device);

//! Compares two Bluetooth device addresses.
//! @return true if the addresses are equal, false if they are not or if one
//! or both addresses were NULL.
bool bt_device_address_equal(const BTDeviceAddress *addr1,
                             const BTDeviceAddress *addr2);

//! Compares the address with an all-zero (invalid) address.
//! @return true if the address is NULL or all-zeroes.
bool bt_device_address_is_invalid(const BTDeviceAddress *addr);

//! Compares two BTDeviceInternal structs.
//! @return true if the devices refer to the same device, false if they refer
//! to different devices or if one or both devices were NULL.
bool bt_device_internal_equal(const BTDeviceInternal *device1_int,
                              const BTDeviceInternal *device2_int);

//! Compares two Bluetooth devices.
//! @return true if the devices refer to the same device, false if they refer
//! to different devices or if one or both devices were NULL.
bool bt_device_equal(const BTDevice *device1, const BTDevice *device2);

//! Tests whether the device is a valid device.
//! This function is meant to be used together with APIs that return a BTDevice,
//! for example ble_service_get_device().
//! @return true if the device appears to be invalid, false if it does not.
bool bt_device_is_invalid(const BTDevice *device);

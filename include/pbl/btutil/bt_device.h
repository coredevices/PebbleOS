/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

#include <stdbool.h>

//! Creates a struct pbl_bt_device given its address.
//! @param address The address to use
//! @param is_random Specify true if the address is a random address, or false
//! if it is the real BD_ADDR of the device.
//! @return The created struct pbl_bt_device
struct pbl_bt_device bt_device_init_with_address(struct pbl_bt_addr address, bool is_random);

//! Gets the address of the device.
//! @param device The device for which to get the address.
//! @return The address of the device.
struct pbl_bt_addr bt_device_get_address(struct pbl_bt_device device);

//! Compares two Bluetooth device addresses.
//! @return true if the addresses are equal, false if they are not or if one
//! or both addresses were NULL.
bool bt_device_address_equal(const struct pbl_bt_addr *addr1, const struct pbl_bt_addr *addr2);

//! Compares the address with an all-zero (invalid) address.
//! @return true if the address is NULL or all-zeroes.
bool bt_device_address_is_invalid(const struct pbl_bt_addr *addr);

//! Compares two struct pbl_bt_device_internal structs.
//! @return true if the devices refer to the same device, false if they refer
//! to different devices or if one or both devices were NULL.
bool bt_device_internal_equal(const struct pbl_bt_device_internal *device1_int,
                              const struct pbl_bt_device_internal *device2_int);

//! Compares two Bluetooth devices.
//! @return true if the devices refer to the same device, false if they refer
//! to different devices or if one or both devices were NULL.
bool bt_device_equal(const struct pbl_bt_device *device1, const struct pbl_bt_device *device2);

//! Tests whether the device is a valid device.
//! This function is meant to be used together with APIs that return a struct pbl_bt_device,
//! for example ble_service_get_device().
//! @return true if the device appears to be invalid, false if it does not.
bool bt_device_is_invalid(const struct pbl_bt_device *device);

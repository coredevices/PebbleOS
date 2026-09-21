/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

//! Gets the UUID for a characteristic.
//! @param characteristic The characteristic for which to get the UUID
//! @return The UUID of the characteristic
Uuid ble_characteristic_get_uuid(pbl_bt_characteristic_t characteristic);

//! Gets the properties bitset for a characteristic.
//! @param characteristic The characteristic for which to get the properties
//! @return The properties bitset
enum pbl_bt_attribute_property ble_characteristic_get_properties(
    pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is readable or not.
//! @param characteristic The characteristic for which to get its readability.
//! @return true if the characteristic is readable, false if it is not.
bool ble_characteristic_is_readable(pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is writable or not.
//! @param characteristic The characteristic for which to get its write-ability.
//! @return true if the characteristic is writable, false if it is not.
bool ble_characteristic_is_writable(pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is writable without response or not.
//! @param characteristic The characteristic for which to get its write-ability.
//! @return true if the characteristic is writable without response, false if it is not.
bool ble_characteristic_is_writable_without_response(pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is subscribable or not.
//! @param characteristic The characteristic for which to get its subscribability.
//! @return true if the characteristic is subscribable, false if it is not.
bool ble_characteristic_is_subscribable(pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is notifiable or not.
//! @param characteristic The characteristic for which to get its notifiability.
//! @return true if the characteristic is notifiable, false if it is not.
bool ble_characteristic_is_notifiable(pbl_bt_characteristic_t characteristic);

//! Gets whether the characteristic is indicatable or not.
//! @param characteristic The characteristic for which to get its indicatability.
//! @return true if the characteristic is indicatable, false if it is not.
bool ble_characteristic_is_indicatable(pbl_bt_characteristic_t characteristic);

//! Gets the service that the characteristic belongs to.
//! @param characteristic The characteristic for which to find the service it
//! belongs to.
//! @return The service owning the characteristic
pbl_bt_service_t ble_characteristic_get_service(pbl_bt_characteristic_t characteristic);

//! Gets the device that the characteristic belongs to.
//! @param characteristic The characteristic for which to find the device it
//! belongs to.
//! @return The device owning the characteristic.
struct pbl_bt_device ble_characteristic_get_device(pbl_bt_characteristic_t characteristic);

//! Gets the descriptors associated with the characteristic.
//! @param characteristic The characteristic for which to get the descriptors
//! @param[out] descriptors_out An array of pointers to descriptors, into which
//! the associated descriptors will be copied.
//! @param num_descriptors The size of the descriptors_out array.
//! @return The total number of descriptors for the service. This might be a
//! larger number than num_descriptors will contain, if the passed array was
//! not large enough to hold all the pointers.
//! @note For convenience, the services are owned by the system and references
//! to services, characteristics and descriptors are guaranteed to remain valid
//! *until the BLEClientServiceChangeHandler is called again* or until
//! application is terminated.
uint8_t ble_characteristic_get_descriptors(pbl_bt_characteristic_t characteristic,
                                           pbl_bt_descriptor_t descriptors_out[],
                                           uint8_t num_descriptors);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// (FUTURE / LATER / NOT SCOPED)
// Just to see how symmetric the Server APIs would be:

pbl_bt_characteristic_t ble_characteristic_create(const Uuid *uuid,
                                                  enum pbl_bt_attribute_property properties);

pbl_bt_characteristic_t ble_characteristic_create_with_descriptors(
    const Uuid *uuid, enum pbl_bt_attribute_property properties, pbl_bt_descriptor_t descriptors[],
    uint8_t num_descriptors);

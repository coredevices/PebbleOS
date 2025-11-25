/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

//! Gets the UUID for a characteristic.
//! @param characteristic The characteristic for which to get the UUID
//! @return The UUID of the characteristic
Uuid ble_characteristic_get_uuid(BLECharacteristic characteristic);

//! Gets the properties bitset for a characteristic.
//! @param characteristic The characteristic for which to get the properties
//! @return The properties bitset
BLEAttributeProperty ble_characteristic_get_properties(BLECharacteristic characteristic);

//! Gets whether the characteristic is readable or not.
//! @param characteristic The characteristic for which to get its readability.
//! @return true if the characteristic is readable, false if it is not.
bool ble_characteristic_is_readable(BLECharacteristic characteristic);

//! Gets whether the characteristic is writable or not.
//! @param characteristic The characteristic for which to get its write-ability.
//! @return true if the characteristic is writable, false if it is not.
bool ble_characteristic_is_writable(BLECharacteristic characteristic);

//! Gets whether the characteristic is writable without response or not.
//! @param characteristic The characteristic for which to get its write-ability.
//! @return true if the characteristic is writable without response, false if it is not.
bool ble_characteristic_is_writable_without_response(BLECharacteristic characteristic);

//! Gets whether the characteristic is subscribable or not.
//! @param characteristic The characteristic for which to get its subscribability.
//! @return true if the characteristic is subscribable, false if it is not.
bool ble_characteristic_is_subscribable(BLECharacteristic characteristic);

//! Gets whether the characteristic is notifiable or not.
//! @param characteristic The characteristic for which to get its notifiability.
//! @return true if the characteristic is notifiable, false if it is not.
bool ble_characteristic_is_notifiable(BLECharacteristic characteristic);

//! Gets whether the characteristic is indicatable or not.
//! @param characteristic The characteristic for which to get its indicatability.
//! @return true if the characteristic is indicatable, false if it is not.
bool ble_characteristic_is_indicatable(BLECharacteristic characteristic);

//! Gets the service that the characteristic belongs to.
//! @param characteristic The characteristic for which to find the service it
//! belongs to.
//! @return The service owning the characteristic
BLEService ble_characteristic_get_service(BLECharacteristic characteristic);

//! Gets the device that the characteristic belongs to.
//! @param characteristic The characteristic for which to find the device it
//! belongs to.
//! @return The device owning the characteristic.
BTDevice ble_characteristic_get_device(BLECharacteristic characteristic);

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
uint8_t ble_characteristic_get_descriptors(BLECharacteristic characteristic,
                                           BLEDescriptor descriptors_out[],
                                           uint8_t num_descriptors);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// (FUTURE / LATER / NOT SCOPED)
// Just to see how symmetric the Server APIs would be:

BLECharacteristic ble_characteristic_create(const Uuid *uuid,
                                            BLEAttributeProperty properties);

BLECharacteristic ble_characteristic_create_with_descriptors(const Uuid *uuid,
                                            BLEAttributeProperty properties,
                                            BLEDescriptor descriptors[],
                                            uint8_t num_descriptors);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

//! Gets the UUID for a descriptor.
//! @param descriptor The descriptor for which to get the UUID.
//! @return The UUID of the descriptor
Uuid ble_descriptor_get_uuid(BLEDescriptor descriptor);

//! Gets the characteristic for a descriptor.
//! @param descriptor The descriptor for which to get the characteristic.
//! @return The characteristic
//! @note For convenience, the services are owned by the system and references
//! to services, characteristics and descriptors are guaranteed to remain valid
//! *until the BLEClientServiceChangeHandler is called again* or until
//! application is terminated.
BLECharacteristic ble_descriptor_get_characteristic(BLEDescriptor descriptor);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// (FUTURE / LATER / NOT SCOPED)
// Just to see how symmetric the Server APIs would be:


BLEDescriptor ble_descriptor_create(const Uuid *uuid,
                                    BLEAttributeProperty properties);

BTErrno ble_descriptor_destroy(BLEDescriptor descriptor);

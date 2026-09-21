/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

//! Gets the UUID for a descriptor.
//! @param descriptor The descriptor for which to get the UUID.
//! @return The UUID of the descriptor
Uuid ble_descriptor_get_uuid(pbl_bt_descriptor_t descriptor);

//! Gets the characteristic for a descriptor.
//! @param descriptor The descriptor for which to get the characteristic.
//! @return The characteristic
//! @note For convenience, the services are owned by the system and references
//! to services, characteristics and descriptors are guaranteed to remain valid
//! *until the BLEClientServiceChangeHandler is called again* or until
//! application is terminated.
pbl_bt_characteristic_t ble_descriptor_get_characteristic(pbl_bt_descriptor_t descriptor);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// (FUTURE / LATER / NOT SCOPED)
// Just to see how symmetric the Server APIs would be:

pbl_bt_descriptor_t ble_descriptor_create(const Uuid *uuid,
                                          enum pbl_bt_attribute_property properties);

enum pbl_bt_errno ble_descriptor_destroy(pbl_bt_descriptor_t descriptor);

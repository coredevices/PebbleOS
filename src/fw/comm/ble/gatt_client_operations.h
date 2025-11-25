/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file This file contains adapter code between Bluetopia's GATT APIs and
//! Pebble's GATT/API code. The functions in this file take the the internal
//! reference types BLECharacteristic and BLEDescriptor to perform operations
//! upon those remote resources. The implementation uses the functions
//! gatt_client_characteristic_get_handle_and_connection_id and
//! gatt_client_descriptor_get_handle_and_connection_id from
//! gatt_client_accessors.c to look up the Bluetopia ConnectionID and the
//! ATT handles. These pieces of information is what Bluetopia cares about
//! when asked to perform a GATT operation.

#include <bluetooth/bluetooth_types.h>

#include "gap_le_task.h"

#define GATT_MTU_MINIMUM (23)

BTErrno gatt_client_op_read(BLECharacteristic characteristic,
                            GAPLEClient client);

void gatt_client_consume_read_response(uintptr_t object_ref,
                                       uint8_t value_out[],
                                       uint16_t value_length,
                                       GAPLEClient client);

BTErrno gatt_client_op_write(BLECharacteristic characteristic,
                             const uint8_t *value,
                             size_t value_length,
                             GAPLEClient client);

BTErrno gatt_client_op_write_without_response(BLECharacteristic characteristic,
                                              const uint8_t *value,
                                              size_t value_length,
                                              GAPLEClient client);

BTErrno gatt_client_op_write_descriptor(BLEDescriptor descriptor,
                                        const uint8_t *value,
                                        size_t value_length,
                                        GAPLEClient client);

BTErrno gatt_client_op_read_descriptor(BLEDescriptor descriptor,
                                       GAPLEClient client);

void gatt_client_op_cleanup(GAPLEClient client);

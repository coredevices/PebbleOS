/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/gatt_service_types.h>

//! @file
//! This file contains functions to access any discovered GATT Services,
//! Characteristics and Descriptors. The data structures are used internally
//! in gatt_client_accessors.c and gatt_client_discovery.c.

typedef struct GATTServiceNode {
  ListNode node;
  struct pbl_bt_gatt_service *service;
} GATTServiceNode;

#define GATTHandleInvalid ((uint16_t)0)

//! Copies the pbl_bt_service_t references for the gatt_remote_services associated
//! with the device.
//! @see prv_handle_service_change in ble_client.c
uint8_t gatt_client_copy_service_refs(const struct pbl_bt_device_internal *device,
                                      pbl_bt_service_t services_out[], uint8_t num_services);

// TODO: add public API to applib
//! Copies the pbl_bt_service_t references for the gatt_remote_services associated
//! with the device, that match a given Service UUID.
//! @note It is possible to have multiple service instances with the same Service UUID.
uint8_t gatt_client_copy_service_refs_matching_uuid(const struct pbl_bt_device_internal *device,
                                                    pbl_bt_service_t services_out[],
                                                    uint8_t num_services,
                                                    const Uuid *matching_service_uuid);

//! Copies the pbl_bt_characteristic_t references associated with the service.
//! @see ble_service_get_characteristics
uint8_t gatt_client_service_get_characteristics(pbl_bt_service_t service_ref,
                                                pbl_bt_characteristic_t characteristics[],
                                                uint8_t num_characteristics);

// TODO: add public API to applib
//! Copies pbl_bt_characteristic_t references associated with the service, filtered by an array of
//! Characteristic UUIDs.
//! @param service_ref The service of which to copy the characteristic references.
//! @param characteristics_out The array into which the matching pbl_bt_characteristic_t references
//! will be copied.
//! @param matching_characteristic_uuids The array of Characteristic Uuids that will be used to
//! determine what references to copy. For every matching characteristic, the reference will be
//! copied into the `characteristics_out` array, at the same index as the Uuid
//! in the `matching_characteristic_uuids` array. The array must contain each Uuid only once.
//! The behavior is undefined when the array contains the same Uuid multiple times.
//! @param num_characteristics The length of both the characteristics_out and
//! matching_characteristic_uuids arrays.
//! @return The number of references that were copied.
//! @note If a characteristic was not found, the element will be set to
//! PBL_BT_CHARACTERISTIC_INVALID. If there were multiple characteristics with the same Uuid, the
//! first one to be found will be copied.
//! @see ble_service_get_characteristics
uint8_t gatt_client_service_get_characteristics_matching_uuids(
    pbl_bt_service_t service_ref, pbl_bt_characteristic_t characteristics_out[],
    const Uuid matching_characteristic_uuids[], uint8_t num_characteristics);

//! Gets the Service UUID associated with the service
//! @see ble_service_get_uuid
Uuid gatt_client_service_get_uuid(pbl_bt_service_t service_ref);

//! Gets the device associated with the service
//! @see ble_service_get_device
struct pbl_bt_device_internal gatt_client_service_get_device(pbl_bt_service_t service_ref);

//! Gets the included services associated with the service
//! @see ble_service_get_included_services
uint8_t gatt_client_service_get_included_services(pbl_bt_service_t service_ref,
                                                  pbl_bt_service_t services_out[],
                                                  uint8_t num_services_out);
//! Gets the UUID of the characteristic
//! @see ble_characteristic_get_uuid
Uuid gatt_client_characteristic_get_uuid(pbl_bt_characteristic_t characteristic);

//! @see ble_characteristic_get_properties
enum pbl_bt_attribute_property gatt_client_characteristic_get_properties(
    pbl_bt_characteristic_t characteristic);

//! @see ble_characteristic_get_service
pbl_bt_service_t gatt_client_characteristic_get_service(pbl_bt_characteristic_t characteristic);

//! @see ble_characteristic_get_device
struct pbl_bt_device_internal gatt_client_characteristic_get_device(
    pbl_bt_characteristic_t characteristic);

//! @see ble_characteristic_get_descriptors
uint8_t gatt_client_characteristic_get_descriptors(pbl_bt_characteristic_t characteristic,
                                                   pbl_bt_descriptor_t descriptors_out[],
                                                   uint8_t num_descriptors);

//! @see ble_descriptor_get_uuid
Uuid gatt_client_descriptor_get_uuid(pbl_bt_descriptor_t descriptor);

//! @see ble_descriptor_get_characteristic
pbl_bt_characteristic_t gatt_client_descriptor_get_characteristic(pbl_bt_descriptor_t descriptor);

bool gatt_client_service_get_handle_range(pbl_bt_service_t service_ref,
                                          struct pbl_bt_att_handle_range *range);

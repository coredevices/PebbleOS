/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/bluetooth/ble_client.h"
#include "comm/ble/kernel_le_client/multi_phone.h"

//! @file ancs.h Module implementing an ANCS client.
//! See http://bit.ly/ancs-spec for Apple's documentation of ANCS

typedef enum {
  ANCSClientStateIdle = 0,
  ANCSClientStateRequestedNotification,
  ANCSClientStateReassemblingNotification,
  ANCSClientStatePerformingAction,
  ANCSClientStateRequestedApp,
  ANCSClientStateAliveCheck,
  ANCSClientStateRetrying,
} ANCSClientState;

//! Enum indexing the ANCS characteristics
//! @note The order is actually important for ancs.c's implementation. Don't shuffle!
typedef enum {
  // Subscribe-able:
  ANCSCharacteristicNotification = 0,  //<! Notification Source
  ANCSCharacteristicData = 1,  //<! Data Source

  // Writable:
  ANCSCharacteristicControl = 2,  //<! Control Point
  NumANCSCharacteristic,

  ANCSCharacteristicInvalid = NumANCSCharacteristic,
} ANCSCharacteristic;

//! Creates the ANCS client for the given phone slot.
//! Must only be called from KernelMain!
void ancs_create(PhoneSlot slot);

//! Updates the BLECharacteristic references, in case new ones have been obtained after a
//! re-discovery of the remote services.
//! @param characteristics Matrix of characteristics references of the ANCS service(s)
//! @param slot            Phone slot (0..MAX_PHONE_CONNECTIONS-1) owning this service
//! @note This module only uses the first service instance, any others will be ignored.
//! Must only be called from KernelMain!
void ancs_handle_service_discovered(BLECharacteristic *characteristics, PhoneSlot slot);

//! Invalidates all ANCS characteristic references for a given phone slot.
void ancs_invalidate_all_references_for_slot(PhoneSlot slot);

//! Invalidates all ANCS characteristic references across all slots.
//! Legacy wrapper kept for callers that don't yet carry slot context.
void ancs_invalidate_all_references(void);

void ancs_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics);

//! @param characteristic The characteristic for which to test whether the ANCS module handles
//! reads/writes/notifications for it.
//! @return True whether the ANCS module handles reads/writes/etc for it, false if not
bool ancs_can_handle_characteristic(BLECharacteristic characteristic);

//! Handles GATT write responses
//! @see BLEClientWriteHandler
//! Must only be called from KernelMain!
void ancs_handle_write_response(BLECharacteristic characteristic, BLEGATTError error);

//! Handles GATT subscriptions
//! @see BLEClientSubscribeHandler
//! Must only be called from KernelMain!
void ancs_handle_subscribe(BLECharacteristic characteristic,
                           BLESubscription subscription_type, BLEGATTError error);

//! Handles GATT notifications
//! Must only be called from KernelMain!
void ancs_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                      size_t value_length, BLEGATTError error);

//! Destroys the ANCS client for the given phone slot.
//! Must only be called from KernelMain!
void ancs_destroy(PhoneSlot slot);

//! This function is safe to call from any task.
void ancs_perform_action(uint32_t notification_uid, uint8_t action_id);

//! Called by kernel_le_client/dis/dis.c
void ancs_handle_ios9_or_newer_detected(void);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ams_types.h"
#include "applib/bluetooth/ble_client.h"

//! @file ams.h Module implementing an AMS client.
//! See http://bit.ly/ams-spec for Apple's documentation of AMS.
//!
//! @note Most of the functions must be called from KernelMain. Forcing all accesses to happen
//! from one task avoids the need for a mutex.

//! Enum indexing the AMS characteristics
//! @note The order is actually important for ams.c's implementation. Don't shuffle!
typedef enum {
  //! Writable.
  //! Used to send commands to the AMS.
  //! @see AMSRemoteCommandID
  AMSCharacteristicRemoteCommand = 0,

  //! Writable w/o Response, Notifiable.
  //! Used to register for attribute updates (by writing w/o response).
  //! Also used to receive attribute updates (as GATT notifications).
  AMSCharacteristicEntityUpdate = 1,

  //! Writable, Readable.
  //! @note Currently left unused. This characteristic is used to fetch a complete value,
  //! in case it got truncated in the update notification.
  AMSCharacteristicEntityAttribute = 2,
  NumAMSCharacteristic,

  AMSCharacteristicInvalid = NumAMSCharacteristic,
} AMSCharacteristic;

//! Creates the AMS client.
//! Must only be called from KernelMain!
void ams_create(void);

//! Updates the BLECharacteristic references, in case new ones have been obtained after a
//! re-discovery of the remote services.
//! @param characteristics Matrix of characteristics references of the AMS service
//! @note This module only uses the first service instance, any others will be ignored.
//! Must only be called from KernelMain!
void ams_handle_service_discovered(BLECharacteristic *characteristics);

void ams_invalidate_all_references(void);

void ams_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics);


//! @param characteristic The characteristic for which to test whether the AMS module handles
//! reads/writes/notifications for it.
//! @return True whether the AMS module handles reads/writes/etc for it, false if not
bool ams_can_handle_characteristic(BLECharacteristic characteristic);

//! Handles GATT subscriptions
//! @see BLEClientSubscribeHandler
//! Must only be called from KernelMain!
void ams_handle_subscribe(BLECharacteristic characteristic,
                           BLESubscription subscription_type, BLEGATTError error);

//! Handles GATT write responses
//! @see BLEClientWriteHandler
//! Must only be called from KernelMain!
void ams_handle_write_response(BLECharacteristic characteristic, BLEGATTError error);

//! Handles GATT notifications
//! Must only be called from KernelMain!
void ams_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                      size_t value_length, BLEGATTError error);

//! Destroys the AMS client.
//! Must only be called from KernelMain!
void ams_destroy(void);

//! This function is exported only for (unit) testing purposes!
//! OK to call from any task.
void ams_send_command(AMSRemoteCommandID command_id);

//! For testing purposes.
//! @return The debug name with which AMS registers itself with the music.c service.
const char *ams_music_server_debug_name(void);

//! For testing purposes.
//! @return Whether AMS has registered itself for updates of all entities (Player, Queue and Track).
bool ams_is_registered_for_all_entity_updates(void);

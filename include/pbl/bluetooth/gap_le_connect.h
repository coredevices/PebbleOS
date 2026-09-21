/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

#include "pbl/kernel/compiler.h"
#include "pbl/bluetooth/hci_types.h"

#include "pbl/kernel/compiler.h"

typedef enum BleAddressType {
  BleAddressType_Public,
  BleAddressType_Random
} BleAddressType;

#ifndef __clang__
_Static_assert(sizeof(BleAddressType) == 1, "BleAddressType is not 1 byte in size");
#endif

// All values in ms
// Used for ConnectionCompleteEvents
typedef struct PBL_PACKED BleConnectionParams {
  uint16_t conn_interval_1_25ms;
  uint16_t slave_latency_events;
  uint16_t supervision_timeout_10ms;
} BleConnectionParams;

// Matches data from "LL_VERSION_IND" - v4.2 2.4.2.13
typedef struct PBL_PACKED BleRemoteVersionInfo {
  uint8_t version_number;
  uint16_t company_identifier;
  uint16_t subversion_number;
} BleRemoteVersionInfo;

typedef struct PBL_PACKED BleRemoteVersionInfoReceivedEvent {
  BTDeviceInternal peer_address;
  BleRemoteVersionInfo remote_version_info;
} BleRemoteVersionInfoReceivedEvent;

// Structs providing data from various Ble Events. I attempted to comment below
// what section of the BT Core Spec more info about the event can be found

// "LE Connection Complete Event" - v4.2 7.7.65.1
typedef struct PBL_PACKED BleConnectionCompleteEvent {
  BleConnectionParams conn_params;
  BTDeviceInternal peer_address;
  HciStatusCode status;
  bool is_master;
  bool is_resolved;
  SMIdentityResolvingKey irk;
  uint16_t handle;
  uint16_t mtu;
} BleConnectionCompleteEvent;

// "Disconnection Complete Event" - v4.2 7.7.5
typedef struct PBL_PACKED BleDisconnectionCompleteEvent {
  BTDeviceInternal peer_address;
  HciStatusCode status;
  HciDisconnectReason reason;
  uint16_t handle;
} BleDisconnectionCompleteEvent;

// "LE Connection Update Complete Event" - v4.2 7.7.65.3
typedef struct PBL_PACKED BleConnectionUpdateCompleteEvent {
  BleConnectionParams conn_params;
  //! Using BTDeviceAddress instead of BTDeviceInternal, because Bluetopia's event doesn't contain
  //! the address type.
  BTDeviceAddress dev_address;
  HciStatusCode status;
} BleConnectionUpdateCompleteEvent; // 7.7.65.3

// Note: This will likely change to work with Dialog
// "Encryption Change Event" - v4.2 7.7.8
typedef struct PBL_PACKED BleEncryptionChange {
  //! Using BTDeviceAddress instead of BTDeviceInternal, because Bluetopia's event doesn't contain
  //! the address type.
  BTDeviceAddress dev_address;
  HciStatusCode status;
  bool encryption_enabled;
} BleEncryptionChange;

typedef struct PBL_PACKED BleAddressChange {
  //! Current device address info.
  BTDeviceInternal device;
  //! New device address info.
  BTDeviceInternal new_device;
} BleAddressChange;

typedef struct PBL_PACKED BleIRKChange {
  //! Current device address info.
  BTDeviceInternal device;
  //! True if the "irk" field is valid
  bool irk_valid;
  //! Identity Resolving Key
  SMIdentityResolvingKey irk;
} BleIRKChange;

//! Bluetooth LE GAP Connection Driver APIs
int pbl_bt_gap_le_disconnect(const BTDeviceInternal *peer_address);

// Callbacks invoked by the backend regarding different BLE Events. It is expected that consumers
// of this module provide an implementation for these callbacks

extern void pbl_bt_handle_le_connection_complete_event(const BleConnectionCompleteEvent *event);
extern void pbl_bt_handle_le_disconnection_complete_event(
    const BleDisconnectionCompleteEvent *event);
extern void pbl_bt_handle_le_encryption_change_event(const BleEncryptionChange *event);
extern void pbl_bt_handle_le_conn_params_update_event(
    const BleConnectionUpdateCompleteEvent *event);
extern void pbl_bt_handle_le_connection_handle_update_address(const BleAddressChange *e);
extern void pbl_bt_handle_le_connection_handle_update_irk(const BleIRKChange *e);
extern void pbl_bt_handle_peer_version_info_event(const BleRemoteVersionInfoReceivedEvent *e);

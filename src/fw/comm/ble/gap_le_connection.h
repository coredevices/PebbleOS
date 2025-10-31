/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "comm/bt_conn_mgr_impl.h"

#include "drivers/rtc.h"

#include "gatt_client_accessors.h"
#include "gatt_client_discovery.h"
#include "gatt_client_subscriptions.h"

#include "services/common/new_timer/new_timer.h"

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/gap_le_connect.h>
#include <bluetooth/sm_types.h>

// FIXME: Including this header results in a compile time failure because the
// chain eventually includes a Bluetopia API. Figure out why this is problematic
// #include "services/common/bluetooth/bluetooth_persistent_storage.h"
// void gap_le_connection_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

// -----------------------------------------------------------------------------
// The calls below are thread-safe, no need to own bt_lock, per se, before
// calling them.

void gap_le_connection_remove(const BTDeviceInternal *device);

bool gap_le_connection_is_connected(const BTDeviceInternal *device);

bool gap_le_connection_is_encrypted(const BTDeviceInternal *device);

uint16_t gap_le_connection_get_gatt_mtu(const BTDeviceInternal *device);

void gap_le_connection_init(void);

void gap_le_connection_deinit(void);

typedef struct DiscoveryJobQueue DiscoveryJobQueue;
typedef struct GAPLEConnectRequestParams GAPLEConnectRequestParams;
typedef struct SMPairingState SMPairingState;

// -----------------------------------------------------------------------------
// The calls below require the caller to own the bt_lock while calling the
// function and for as long as the result is being used / accessed.

typedef struct GAPLEConnection {
  ListNode node;

  //! The remote device its (connection) address.
  BTDeviceInternal device;

  //! Whether we are the master for this connection.
  bool local_is_master:1;

  //! Whether the connection is encrypted or not.
  bool is_encrypted:1;

  //! Whether GATT service discovery is in progress
  bool gatt_is_service_discovery_in_progress:1;

  //! Whether the connected device is our gateway (aka "the phone running Pebble app")
  bool is_gateway:1;

  //! @see pebble_pairing_service.c
  bool is_subscribed_to_connection_status_notifications:1;
  bool is_subscribed_to_gatt_mtu_notifications:1;

  //! Whether the device is subscribed to heart rate monitor value updates (the other device has
  //! enabled the "Notifications" bit of the CCCD).
  bool hrm_service_is_subscribed:1;

  //! The number of service discovery retries.
  //! See field `gatt_service_discovery_watchdog_timer`
  uint8_t gatt_service_discovery_retries:GATT_CLIENT_DISCOVERY_MAX_RETRY_BITS;

  //! The generation number of the remote services that have been discovered.
  uint8_t gatt_service_discovery_generation;

  //! Bluetopia's internal identifier for the GATT connection.
  //! This is not a concept that can be found in the Bluetooth specification,
  //! it's internal to Bluetopia.
  uintptr_t gatt_connection_id;

  //! Maximum Transmission Unit. "The maximum size of payload data, in octets,
  //! that the upper layer entity is capable of accepting."
  uint16_t gatt_mtu;

  //! The ATT handle of the "Service Changed" characteristic.
  //! See gatt_service_changed.c
  uint16_t gatt_service_changed_att_handle;
  bool has_sent_gatt_service_changed_indication;
  TimerID gatt_service_changed_indication_timer;

  //! The bonding ID (only for BLE at the moment).
  //! If the device is not bonded, the field will be BT_BONDING_ID_INVALID
  BTBondingID bonding_id;

  //! The IRK of the remote device, NULL if the connection address was not resolved.
  SMIdentityResolvingKey *irk;

  //! @see gap_le_device_name.c
  char *device_name;

  //! List of services that have been discovered on the remote device.
  GATTServiceNode *gatt_remote_services;

  //! List of subscriptions to notifications/
  GATTClientSubscriptionNode *gatt_subscriptions;

  //! Temporary, connection related pairing data (Bluetopia/cc2564 only)
  SMPairingState *pairing_state;

  //! Opaque, used by bt_conn_mgr to decide speed connection should run at
  ConnectionMgrInfo *conn_mgr_info;

  //! Opaque, used by gatt_client_discovery.c
  DiscoveryJobQueue *discovery_jobs;

  //! @see gap_le_connect_params.c
  struct {
    TimerID watchdog_timer;
    uint8_t attempts;
    bool is_request_pending;
  } param_update_info;

  //! Current BLE connection parameter cache
  BleConnectionParams conn_params;

  //! Contains the BT chip version info for the remote device if available (all 0's if not)
  BleRemoteVersionInfo remote_version_info;

  //! @see pebble_pairing_service.h for info on these fields:
  bool is_remote_device_managing_connection_parameters;
  //! Custom connection parameter sets for each ResponseTimeState, as written by the remote through
  //! the Pebble Pairing Service. Can be NULL if the remote has never written any.
  GAPLEConnectRequestParams *connection_parameter_sets;

  RtcTicks ticks_since_connection;
} GAPLEConnection;


GAPLEConnection *gap_le_connection_add(const BTDeviceInternal *device,
                                       const SMIdentityResolvingKey *irk,
                                       bool local_is_master,
                                       TimerID param_watchdog_timer);

//! Checks to see if the LE connection is in our list of currently tracked
//! connections
bool gap_le_connection_is_valid(const GAPLEConnection *conn);

//! Find the first GAPLEConnection
//! Added for legacy support (pp_ble_control_legacy.c)
GAPLEConnection *gap_le_connection_any(void);

//! Find the GAPLEConnection by device.
//! @note !!! To access the returned context bt_lock MUST be held!!!
GAPLEConnection *gap_le_connection_by_device(const BTDeviceInternal *device);

//! Find the GAPLEConnection by Bluetooth device address.
//! @note !!! To access the returned context bt_lock MUST be held!!!
//! @note Bluetopia's GATT API seems to make no difference between public /
//! private addresses. Therefore, this function does not take a BTDevice.
GAPLEConnection *gap_le_connection_by_addr(const BTDeviceAddress *addr);

//! Find the GAPLEConnection by Bluetopia GATT ConnectionID.
//! @note !!! To access the returned context bt_lock MUST be held!!!
GAPLEConnection *gap_le_connection_by_gatt_id(unsigned int connection_id);

//! Find the GAPLEConnection by IRK.
GAPLEConnection *gap_le_connection_find_by_irk(const SMIdentityResolvingKey *irk);

typedef bool (*GAPLEConnectionFindCallback)(GAPLEConnection *connection,
                                            void *data);

GAPLEConnection *gap_le_connection_find(GAPLEConnectionFindCallback filter,
                                        void *data);

typedef void (*GAPLEConnectionForEachCallback)(GAPLEConnection *connection,
                                               void *data);

void gap_le_connection_for_each(GAPLEConnectionForEachCallback cb, void *data);

//! @note deep-copies the IRK.
void gap_le_connection_set_irk(GAPLEConnection *connection, const SMIdentityResolvingKey *irk);

//! Sets whether the connection is to the gateway device (aka "the phone").
//! Updates the is_gateway flag on any associated bonding as well.
void gap_le_connection_set_gateway(GAPLEConnection *connection, bool is_gateway);

GAPLEConnection *gap_le_connection_get_gateway(void);

void gap_le_connection_copy_device_name(
    const GAPLEConnection *connection, char *name_out, size_t namelen);

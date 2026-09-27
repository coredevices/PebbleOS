/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

#include "pbl/kernel/compiler.h"
#include "pbl/bluetooth/hci_types.h"

#include "pbl/kernel/compiler.h"

enum pbl_bt_addr_type {
  PBL_BT_ADDR_TYPE_PUBLIC,
  PBL_BT_ADDR_TYPE_RANDOM
};

#ifndef __clang__
_Static_assert(sizeof(enum pbl_bt_addr_type) == 1, "enum pbl_bt_addr_type is not 1 byte in size");
#endif

// All values in ms
// Used for ConnectionCompleteEvents
struct PBL_PACKED pbl_bt_conn_params {
  uint16_t conn_interval_1_25ms;
  uint16_t slave_latency_events;
  uint16_t supervision_timeout_10ms;
};

// Matches data from "LL_VERSION_IND" - v4.2 2.4.2.13
struct PBL_PACKED pbl_bt_remote_version_info {
  uint8_t version_number;
  uint16_t company_identifier;
  uint16_t subversion_number;
};

struct PBL_PACKED pbl_bt_remote_version_info_received_event {
  struct pbl_bt_device_internal peer_address;
  struct pbl_bt_remote_version_info remote_version_info;
};

// Structs providing data from various Ble Events. I attempted to comment below
// what section of the BT Core Spec more info about the event can be found

// "LE Connection Complete Event" - v4.2 7.7.65.1
struct PBL_PACKED pbl_bt_conn_complete_event {
  struct pbl_bt_conn_params conn_params;
  struct pbl_bt_device_internal peer_address;
  enum pbl_bt_hci_status status;
  bool is_master;
  bool is_resolved;
  struct pbl_bt_sm_key irk;
  uint16_t handle;
  uint16_t mtu;
};

// "Disconnection Complete Event" - v4.2 7.7.5
struct PBL_PACKED pbl_bt_disconn_complete_event {
  struct pbl_bt_device_internal peer_address;
  enum pbl_bt_hci_status status;
  enum pbl_bt_hci_status reason;
  uint16_t handle;
};

// "LE Connection Update Complete Event" - v4.2 7.7.65.3
struct PBL_PACKED pbl_bt_conn_update_complete_event {
  struct pbl_bt_conn_params conn_params;
  //! Using struct pbl_bt_addr instead of struct pbl_bt_device_internal, because Bluetopia's event
  //! doesn't contain the address type.
  struct pbl_bt_addr dev_address;
  enum pbl_bt_hci_status status;
}; // 7.7.65.3

// Note: This will likely change to work with Dialog
// "Encryption Change Event" - v4.2 7.7.8
struct PBL_PACKED pbl_bt_encryption_change {
  //! Using struct pbl_bt_addr instead of struct pbl_bt_device_internal, because Bluetopia's event
  //! doesn't contain the address type.
  struct pbl_bt_addr dev_address;
  enum pbl_bt_hci_status status;
  bool encryption_enabled;
};

struct PBL_PACKED pbl_bt_addr_change {
  //! Current device address info.
  struct pbl_bt_device_internal device;
  //! New device address info.
  struct pbl_bt_device_internal new_device;
};

struct PBL_PACKED pbl_bt_irk_change {
  //! Current device address info.
  struct pbl_bt_device_internal device;
  //! True if the "irk" field is valid
  bool irk_valid;
  //! Identity Resolving Key
  struct pbl_bt_sm_key irk;
};

//! Bluetooth LE GAP Connection Driver APIs
int pbl_bt_gap_le_disconnect(const struct pbl_bt_device_internal *peer_address);

// Callbacks invoked by the backend regarding different BLE Events. It is expected that consumers
// of this module provide an implementation for these callbacks

extern void pbl_bt_handle_le_connection_complete_event(
    const struct pbl_bt_conn_complete_event *event);
extern void pbl_bt_handle_le_disconnection_complete_event(
    const struct pbl_bt_disconn_complete_event *event);
extern void pbl_bt_handle_le_encryption_change_event(const struct pbl_bt_encryption_change *event);
extern void pbl_bt_handle_le_conn_params_update_event(
    const struct pbl_bt_conn_update_complete_event *event);
extern void pbl_bt_handle_le_connection_handle_update_address(const struct pbl_bt_addr_change *e);
extern void pbl_bt_handle_le_connection_handle_update_irk(const struct pbl_bt_irk_change *e);
extern void pbl_bt_handle_peer_version_info_event(
    const struct pbl_bt_remote_version_info_received_event *e);

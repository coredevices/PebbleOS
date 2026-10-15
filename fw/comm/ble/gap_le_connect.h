/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>

#define GAP_LE_CONNECT_MASTER_MAX_CONNECTION_INTENTS (5)

//! Internal extensions to the standard HCI status values (see HCITypes.h)
typedef enum {
  //! The virtual connection was disconnected because the user removed the bonding.
  GAPLEConnectHCIReasonExtensionUserRemovedBonding = 0xFB,
  //! The virtual connection was disconnected because the client called
  //! gap_le_connect_cancel().
  GAPLEConnectHCIReasonExtensionCancelConnect = 0xFC,
  //! The virtual connection was disconnected because the system went into
  //! airplane mode.
  GAPLEConnectHCIReasonExtensionAirPlaneMode = 0xFD,
} GAPLEConnectHCIReasonExtension;

void gap_le_connect_init(void);

void gap_le_connect_deinit(void);

bool gap_le_connect_is_connected_as_slave(void);

void gap_le_connect_handle_bonding_change(pbl_bt_bonding_id_t bonding, BtPersistBondingOp op);

enum pbl_bt_errno gap_le_connect_connect(const struct pbl_bt_device_internal *device,
                                         bool auto_reconnect, bool is_pairing_required);

enum pbl_bt_errno gap_le_connect_cancel(const struct pbl_bt_device_internal *device);

enum pbl_bt_errno gap_le_connect_connect_by_bonding(pbl_bt_bonding_id_t bonding_id,
                                                    bool auto_reconnect, bool is_pairing_required);

enum pbl_bt_errno gap_le_connect_cancel_by_bonding(pbl_bt_bonding_id_t bonding_id);

//! @note As opposed to gap_le_connect_cancel(), this function will not
//! generate virtual disconnection events for any connected devices.
void gap_le_connect_cancel_all(void);

// -------------------------------------------------------------------------------------------------
// For unit testing

bool gap_le_connect_has_pending_create_connection(void);

//! @return true if there is a connection intent for the specified device.
bool gap_le_connect_has_connection_intent(const struct pbl_bt_device_internal *device);

bool gap_le_connect_has_connection_intent_for_bonding(pbl_bt_bonding_id_t bonding_id);

uint32_t gap_le_connect_connection_intents_count(void);

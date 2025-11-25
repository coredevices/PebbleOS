/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#include "gap_le_task.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"

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

void gap_le_connect_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

BTErrno gap_le_connect_connect(const BTDeviceInternal *device, bool auto_reconnect,
                                      bool is_pairing_required, GAPLEClient client);

BTErrno gap_le_connect_cancel(const BTDeviceInternal *device, GAPLEClient client);

BTErrno gap_le_connect_connect_by_bonding(BTBondingID bonding_id, bool auto_reconnect,
                                          bool is_pairing_required, GAPLEClient client);

BTErrno gap_le_connect_cancel_by_bonding(BTBondingID bonding_id, GAPLEClient client);

//! @note As opposed to gap_le_connect_cancel(), this function will not
//! generate virtual disconnection events for any connected devices.
//! This is because this function is used by the kernel to clean up after the client (app)
//! when it is in the process of terminating.
void gap_le_connect_cancel_all(GAPLEClient client);

// -------------------------------------------------------------------------------------------------
// For unit testing

bool gap_le_connect_has_pending_create_connection(void);

//! @return true if there is a connection intent for the specified device and
//! specified client.
bool gap_le_connect_has_connection_intent(const BTDeviceInternal *device,
                                                 GAPLEClient client);

bool gap_le_connect_has_connection_intent_for_bonding(BTBondingID bonding_id,
                                                      GAPLEClient c);

uint32_t gap_le_connect_connection_intents_count(void);

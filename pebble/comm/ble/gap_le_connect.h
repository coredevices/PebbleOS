/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/types.h>

//! Reasons of PEBBLE_BLE_CONNECTION_EVENT that are not HCI's.
typedef enum {
  GAPLEConnectHCIReasonExtensionUserRemovedBonding = 0xFB,
} GAPLEConnectHCIReasonExtension;

void gap_le_connect_deinit(void);

bool gap_le_connect_is_connected_as_slave(void);

//! A bond was made: its connection, once encrypted, is the gateway's, reported with
//! PEBBLE_BLE_CONNECTION_EVENT.
void gap_le_connect_handle_bond_added(const struct pbl_bt_bond_info *info);

//! A bond was deleted: its connection is dropped, and reported as lost.
void gap_le_connect_handle_bond_deleted(const struct pbl_bt_device_internal *addr);

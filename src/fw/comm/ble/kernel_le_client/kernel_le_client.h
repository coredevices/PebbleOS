/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"
#include "comm/ble/kernel_le_client/multi_phone.h"

//! @file kernel_le_client.h
//! Module that is responsible of connecting to the BLE gateway (aka "the phone") in order to:
//! - bootstrap the Pebble Protocol over GATT (PPoGATT) module
//! - bootstrap the ANCS module
//! - bootstrap the "Service Changed" module

void kernel_le_client_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

//! Returns true if the given slot is connected to the primary gateway phone.
bool kernel_le_client_is_gateway_slot(PhoneSlot slot);

//! Explicitly set a bonding as the preferred gateway. Persists across reboots.
//! If the bonding is currently connected, switches the active gateway slot immediately.
void kernel_le_client_set_active_gateway(BTBondingID bonding_id);

void kernel_le_client_handle_event(const PebbleEvent *event);

void kernel_le_client_init(void);

void kernel_le_client_deinit(void);

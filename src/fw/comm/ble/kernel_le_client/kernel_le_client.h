/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"

//! @file kernel_le_client.h
//! Module that is responsible of connecting to the BLE gateway (aka "the phone") in order to:
//! - bootstrap the Pebble Protocol over GATT (PPoGATT) module
//! - bootstrap the ANCS module
//! - bootstrap the "Service Changed" module

void kernel_le_client_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

void kernel_le_client_handle_event(const PebbleEvent *event);

void kernel_le_client_init(void);

void kernel_le_client_deinit(void);

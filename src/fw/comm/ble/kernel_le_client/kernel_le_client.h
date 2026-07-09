/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"

//! @file kernel_le_client.h
//! Module that is responsible of connecting to the BLE gateway (aka "the phone") in order to:
//! - bootstrap the Pebble Protocol over GATT (PPoGATT) module
//! - bootstrap the ANCS module
//! - bootstrap the "Service Changed" module

void kernel_le_client_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

void kernel_le_client_handle_event(const PebbleEvent *event);

//! When a peer exposes more than one instance of a service (e.g. a buggy
//! companion that left a stale duplicate behind), we hand the client a single
//! instance and rotate which one across reconnects until a handshake succeeds.
//! PPoGATT calls these to drive that rotation: reset on a successful session,
//! advance after a failed handshake attempt. See prv_handle_services_added.
void kernel_le_client_reset_service_instance_attempt(void);
void kernel_le_client_advance_service_instance_attempt(void);

void kernel_le_client_init(void);

void kernel_le_client_deinit(void);

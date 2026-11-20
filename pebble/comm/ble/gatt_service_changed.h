/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file
//! This module contains the "Generic Attribute Profile Service" code, both the server and
//! client parts. Both ends can optionally implement this service (and client). iOS does for example
//! and so does Pebble. The one characteristic this service has is called "Service Changed". Its
//! purpose is to indicate to the other side whenever there are changes to the local GATT database
//! (and what ATT handle range the change is affecting), for example when an app adds or removes
//! a GATT service or characteristics.
//!
//! The server is mostly implemented in Bluetopia's GATT.c, but relies on our FW for some mundane
//! things like handling subscription events and actually firing off "Service Changed" indications.
//!
//! The client of the remote's characteristic is with the clients of the gateway's services, see
//! kernel_le_client.c.
//!
//! See BT Spec 4.0, Volume 3, Part G, 7.1 "Service Changed" for more information about the service.

////////////////////////////////////////////////////////////////////////////////////////////////////
// Server -- Pebble serving up the "Service Changed" characteristic to the remote

void gatt_service_changed_server_handle_fw_update(void);

////////////////////////////
// For Testing:

// BLEChrIdx gatt_service_changed_get_characteristic_idx(void);

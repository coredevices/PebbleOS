/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/gatt_service_types.h>

typedef struct GAPLEConnection GAPLEConnection;
struct pbl_bt_gatt_service;
typedef struct GATTServiceNode GATTServiceNode;

enum pbl_bt_errno pbl_bt_gatt_start_discovery_range(const GAPLEConnection *connection,
                                                    const struct pbl_bt_att_handle_range *data);
enum pbl_bt_errno pbl_bt_gatt_stop_discovery(GAPLEConnection *connection);

//! It's possible we are disconnected or the stack gets torn down while in the
//! middle of a discovery. This routine gets invoked if the connection gets
//! torn down or goes away so that the implementation can clean up any tracking
//! it has waiting for a discovery to complete
void pbl_bt_gatt_handle_discovery_abandoned(void);

//! gatt_service_discovery callbacks
//! cb returns true iff the driver completed, false if a discovery retry was initiated
extern bool pbl_bt_cb_gatt_client_discovery_complete(GAPLEConnection *connection,
                                                     enum pbl_bt_errno errno);
extern void pbl_bt_cb_gatt_client_discovery_handle_indication(
    GAPLEConnection *connection, struct pbl_bt_gatt_service *service_discovered,
    enum pbl_bt_errno error);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

#include <comm/ble/gap_le_connection.h>

PBL_WEAK void gatt_client_discovery_cleanup_by_connection(struct GAPLEConnection *connection,
                                                          enum pbl_bt_errno reason) {
}

PBL_WEAK void gatt_client_cleanup_discovery_jobs(GAPLEConnection *connection) {
}

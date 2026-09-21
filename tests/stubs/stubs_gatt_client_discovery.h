/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct GAPLEConnection;

void gatt_client_discovery_cleanup_by_connection(struct GAPLEConnection *connection,
                                                 enum pbl_bt_errno reason) {
}

void gatt_client_cleanup_discovery_jobs(GAPLEConnection *connection) {
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_client_discovery.h"

#include "comm/ble/gap_le_connection.h"

void gatt_client_discovery_cleanup_by_connection(GAPLEConnection *connection) { }

void gatt_client_subscription_cleanup_by_att_handle_range(
    struct GAPLEConnection *connection, ATTHandleRange *range) { }

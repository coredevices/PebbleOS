/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>
#include <pbl/kernel/compiler.h>

struct GAPLEConnection;

PBL_WEAK void gatt_client_subscriptions_cleanup_by_connection(struct GAPLEConnection *connection,
                                                              bool should_unsubscribe) {
}

PBL_WEAK void gatt_client_subscription_cleanup_by_att_handle_range(
    struct GAPLEConnection *connection, struct pbl_bt_att_handle_range *range) {
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>
#include <pbl/bluetooth/types.h>

// -------------------------------------------------------------------------------------------------
// Gatt Client Discovery API calls

enum pbl_bt_errno pbl_bt_gatt_start_discovery_range(const GAPLEConnection *connection,
                                                    const struct pbl_bt_att_handle_range *data) {
  return 0;
}

enum pbl_bt_errno pbl_bt_gatt_stop_discovery(GAPLEConnection *connection) {
  return 0;
}

void pbl_bt_gatt_handle_discovery_abandoned(void) {
}

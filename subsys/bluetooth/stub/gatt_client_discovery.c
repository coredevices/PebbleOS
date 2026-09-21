/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>
#include <pbl/bluetooth/types.h>

#include <inttypes.h>

// -------------------------------------------------------------------------------------------------
// Gatt Client Discovery API calls

BTErrno pbl_bt_gatt_start_discovery_range(const GAPLEConnection *connection,
                                          const ATTHandleRange *data) {
  return 0;
}

BTErrno pbl_bt_gatt_stop_discovery(GAPLEConnection *connection) {
  return 0;
}

void pbl_bt_gatt_handle_discovery_abandoned(void) {
}

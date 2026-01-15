/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "stubs_bt_driver_gatt_client_discovery.h"

// TODO: Rethink how we want to stub out these new driver wrapper calls.

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection, const ATTHandleRange *data) {
  return BTErrnoOK;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  return BTErrnoOK;
}

void bt_driver_gatt_handle_finalize_discovery(GAPLEConnection *connection) {
}

void bt_driver_gatt_handle_discovery_abandoned(void) {
}

uint32_t bt_driver_gatt_get_watchdog_timer_id(void) {
  return 0;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/gatt.h>

// TODO: Rethink how we want to stub out these new driver wrapper calls.

static inline BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection, const ATTHandleRange *data) {
  return BTErrnoOK;
}

static inline BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  return BTErrnoOK;
}

static inline void bt_driver_gatt_handle_finalize_discovery(GAPLEConnection *connection) {
}

static inline void bt_driver_gatt_handle_discovery_abandoned(void) {}

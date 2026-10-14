/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <comm/ble/gap_le_connection.h>

PBL_WEAK void pebble_pairing_service_deinit(unsigned int stack_id) {
}

PBL_WEAK bool pebble_pairing_service_init(unsigned int stack_id) {
  return true;
}

PBL_WEAK void pebble_pairing_service_handle_status_change(const GAPLEConnection *connection) {
}

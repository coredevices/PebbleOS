/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_connection.h"

void pebble_pairing_service_deinit(unsigned int stack_id) {
}

bool pebble_pairing_service_init(unsigned int stack_id) {
  return true;
}

void pebble_pairing_service_handle_status_change(const GAPLEConnection *connection) {
}

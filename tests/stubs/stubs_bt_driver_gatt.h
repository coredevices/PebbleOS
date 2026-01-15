/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/gatt.h>

// TODO: Rethink how we want to stub out these new driver wrapper calls.

void bt_driver_gatt_send_changed_indication(uint32_t connection_id, const ATTHandleRange *data) {
  // Stub implementation - does nothing
}

void bt_driver_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
  // Stub implementation - does nothing
}

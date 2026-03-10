/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Include fake_GAPAPI.h first to get type definitions and function declarations
// This is needed because some test files include this stub without including fake_GAPAPI.h
#include "fake_GAPAPI.h"

#include "bluetooth/bluetooth_types.h"  // For BLEAdData definition

// NOTE: gap_le_advert_handle_connect_as_slave and gap_le_advert_handle_disconnect_as_slave
// are already defined in src/fw/comm/ble/gap_le_advert.c, so we don't stub them here.

// Bluetooth driver advertising functions
// NOTE: These are not static inline to allow linking from compiled source files
bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms) {
  return true;
}

void bt_driver_advert_advertising_disable(void) {
}

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) {
  if (tx_power) {
    *tx_power = 0;
  }
  return true;
}

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
  // Stub implementation - no-op
  (void)ad_data;
}


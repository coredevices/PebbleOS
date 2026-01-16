/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct BLEAdData BLEAdData;

// NOTE: gap_le_advert_handle_connect_as_slave and gap_le_advert_handle_disconnect_as_slave
// are already defined in src/fw/comm/ble/gap_le_advert.c, so we don't stub them here.

// Bluetooth driver advertising functions
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
  // Stub implementation - advertising data is set via GAP_LE_Set_Advertising_Data
  // which is called by the production code in gap_le_advert.c
}


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
  // Call the GAP LE API functions to set advertising data in tests
  // Functions are declared at top of this file
  GAP_LE_Set_Advertising_Data(0, ad_data->ad_data_length,
                             (Advertising_Data_t *)ad_data->data);

  if (ad_data->scan_resp_data_length > 0) {
    GAP_LE_Set_Scan_Response_Data(0, ad_data->scan_resp_data_length,
                                   (Scan_Response_Data_t *)(ad_data->data + ad_data->ad_data_length));
  }
}


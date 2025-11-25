/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/bt_driver_advert.h>

void bt_driver_advert_advertising_disable(void) {
}

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) {
  return false;
}

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
}

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms) {
  return false;
}
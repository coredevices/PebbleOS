/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/advert.h>

void pbl_bt_advert_advertising_disable(void) {
}

bool pbl_bt_advert_client_get_tx_power(int8_t *tx_power) {
  return false;
}

bool pbl_bt_advert_set_advertising_data(const struct pbl_bt_ad_data *ad_data) {
  return false;
}

bool pbl_bt_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms) {
  return false;
}

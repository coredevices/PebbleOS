/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/bt_driver_advert.h>

void bt_driver_advert_advertising_disable(void) {}

bool bt_driver_advert_is_connectable(void) { return false; }

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) { return false; }

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {}

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                         bool enable_scan_resp) {
  return false;
}

bool bt_driver_advert_client_has_cycled(void) { return false; }

void bt_driver_advert_client_set_cycled(bool has_cycled) {}

bool bt_driver_advert_should_not_cycle(void) { return false; }

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
#include <system/logging.h>
#include <system/passert.h>

#include <host/ble_gap.h>

void bt_driver_advert_advertising_disable(void) {
  int rc;

  if (ble_gap_adv_active() == 0) {
    return;
  }

  rc = ble_gap_adv_stop();
  PBL_ASSERT(rc == 0, "Failed to stop advertising (%d)", rc);
}

// no impl needed for nimble, buggy stack workaround
bool bt_driver_advert_is_connectable(void) { return true; }

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) { return false; }

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
  int rc;

  rc = ble_gap_adv_set_data((uint8_t *)&ad_data->data, ad_data->ad_data_length);
  PBL_ASSERT(rc == 0, "Failed to set advertising data (%d)", rc);

  rc = ble_gap_adv_rsp_set_data((uint8_t *)&ad_data->data[ad_data->ad_data_length],
                                ad_data->scan_resp_data_length);
  PBL_ASSERT(rc == 0, "Failed to set scan response data (%d)", rc);
}

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                         bool enable_scan_resp) {
  int rc;
  uint8_t own_addr_type;
  struct ble_gap_adv_params advp = {
      .conn_mode = enable_scan_resp ? BLE_GAP_CONN_MODE_UND : BLE_GAP_DISC_MODE_NON,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
      .itvl_min = BLE_GAP_CONN_ITVL_MS(min_interval_ms),
      .itvl_max = BLE_GAP_CONN_ITVL_MS(max_interval_ms),
  };

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to infer own address type (%d)", rc);
    return false;
  }

  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &advp, NULL, NULL);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to start advertising (%d)", rc);
    return false;
  }

  return true;
}

bool bt_driver_advert_client_has_cycled(void) { return false; }

void bt_driver_advert_client_set_cycled(bool has_cycled) {}

bool bt_driver_advert_should_not_cycle(void) { return false; }

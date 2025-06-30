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

#include <bluetooth/responsiveness.h>
#include <host/ble_gap.h>

#include "nimble_type_conversions.h"

bool bt_driver_le_connection_parameter_update(const BTDeviceInternal *addr,
                                              const BleConnectionParamsUpdateReq *req) {
  ble_addr_t nimble_addr;
  struct ble_gap_conn_desc desc;
  struct ble_gap_upd_params params = { 0 };

  pebble_device_to_nimble_addr(addr, &nimble_addr);

  int rc = ble_gap_conn_find_by_addr(&nimble_addr, &desc);
  if (rc != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "ble_gap_conn_find_by_addr failed: %d", rc);
    return false;
  }

  pebble_conn_update_to_nimble(req, &params);

  rc = ble_gap_update_params(desc.conn_handle, &params);
  if (rc != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "ble_gap_update_params failed: 0x%04x", (uint16_t)rc);
    return false;
  }
  return true;
}

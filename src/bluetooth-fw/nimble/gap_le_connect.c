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

#include <bluetooth/gap_le_connect.h>
#include <host/ble_gap.h>

#include "nimble_type_conversions.h"

int bt_driver_gap_le_disconnect(const BTDeviceInternal *peer_address) {
  uint16_t conn_handle;

  if (!pebble_device_to_nimble_conn_handle(peer_address, &conn_handle)) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "bt_driver_gap_le_disconnect: Failed to find connection handle");
    return -1;
  }

  int rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "ble_gap_terminate rc=0x%04x", (uint16_t)rc);
  }

  return rc;
}

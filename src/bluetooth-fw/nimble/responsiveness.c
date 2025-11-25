/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/responsiveness.h>
#include <host/ble_gap.h>

#include "nimble_type_conversions.h"

bool bt_driver_le_connection_parameter_update(const BTDeviceInternal *addr,
                                              const BleConnectionParamsUpdateReq *req) {
  ble_addr_t nimble_addr;
  struct ble_gap_conn_desc desc;
  struct ble_gap_upd_params params;
  int rc;

  pebble_device_to_nimble_addr(addr, &nimble_addr);

  rc = ble_gap_conn_find_by_addr(&nimble_addr, &desc);
  if (rc != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "ble_gap_conn_find_by_addr failed: %d", rc);
    return false;
  }

  pebble_conn_update_to_nimble(req, &params);

  PBL_LOG(LOG_LEVEL_DEBUG,
          "Request connection parameters: "
          "interval=(%u, %u) ms, latency=%u, spvn timeout=%u ms",
          params.itvl_min * BLE_HCI_CONN_ITVL / 1000, params.itvl_max * BLE_HCI_CONN_ITVL / 1000,
          params.latency, params.supervision_timeout * BLE_HCI_CONN_SPVN_TMO_UNITS);

  rc = ble_gap_update_params(desc.conn_handle, &params);
  if (rc != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "ble_gap_update_params failed: 0x%04x", (uint16_t)rc);
    return false;
  }

  return true;
}

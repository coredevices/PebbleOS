/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/analytics.h>
#include <host/ble_gap.h>

#include "nimble_type_conversions.h"

bool bt_driver_analytics_get_connection_quality(const BTDeviceInternal *address,
                                                 uint8_t *link_quality, int8_t *rssi) {
  if (address->is_classic) {
    return false;
  }

  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(address, &conn_handle)) {
    return false;
  }

  int rc = ble_gap_conn_rssi(conn_handle, rssi);
  if (rc != 0) {
    return false;
  }

  // Set link quality based on RSSI (0 = poor, 255 = excellent)
  if (*rssi >= -50) {
    *link_quality = 255;
  } else if (*rssi >= -70) {
    *link_quality = 170;
  } else if (*rssi >= -85) {
    *link_quality = 85;
  } else {
    *link_quality = 0;
  }

  return true;
}

bool bt_driver_analytics_collect_ble_parameters(const BTDeviceInternal *addr,
                                                LEChannelMap *le_chan_map_res) {
  return false;
}

void bt_driver_analytics_external_collect_chip_specific_parameters(void) {}

void bt_driver_analytics_external_collect_bt_chip_heartbeat(void) {}

bool bt_driver_analytics_get_conn_event_stats(SlaveConnEventStats *stats) {
  return false;  // Info not available
}

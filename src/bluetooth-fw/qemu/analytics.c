/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/analytics.h>
#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>
#include <stdint.h>

#include "comm/ble/gap_le_connection.h"

bool bt_driver_analytics_get_connection_quality(const BTDeviceInternal *address,
                                                uint8_t *link_quality_out, int8_t *rssi_out) {
  return false;
}

bool bt_driver_analytics_collect_ble_parameters(const BTDeviceInternal *addr,
                                                LEChannelMap *le_chan_map_res) {
  return false;
}

void bt_driver_analytics_external_collect_chip_specific_parameters(void) {
}

void bt_driver_analytics_external_collect_bt_chip_heartbeat(void) {
}

bool bt_driver_analytics_get_conn_event_stats(SlaveConnEventStats *stats) {
  return false; // Info not available
}

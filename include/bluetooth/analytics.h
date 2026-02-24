/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/attributes.h"

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/conn_event_stats.h>

#define NUM_LE_CHANNELS 37

typedef struct PACKED LEChannelMap {
  uint8_t byte0;
  uint8_t byte1;
  uint8_t byte2;
  uint8_t byte3;
  uint8_t byte4;
} LEChannelMap;

bool bt_driver_analytics_get_connection_quality(const BTDeviceInternal *address,
                                                uint8_t *link_quality_out, int8_t *rssi_out);

bool bt_driver_analytics_collect_ble_parameters(const BTDeviceInternal *addr,
                                                LEChannelMap *le_chan_map_res);

void bt_driver_analytics_external_collect_chip_specific_parameters(void);

void bt_driver_analytics_external_collect_bt_chip_heartbeat(void);

//! Returns true iff there are connection event stats to report
bool bt_driver_analytics_get_conn_event_stats(SlaveConnEventStats *stats);

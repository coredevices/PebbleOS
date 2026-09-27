/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pbl/kernel/compiler.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/conn_event_stats.h>

#define PBL_BT_NUM_LE_CHANNELS 37

struct PBL_PACKED pbl_bt_le_channel_map {
  uint8_t byte0;
  uint8_t byte1;
  uint8_t byte2;
  uint8_t byte3;
  uint8_t byte4;
};

bool pbl_bt_analytics_get_connection_quality(const struct pbl_bt_device_internal *address,
                                             uint8_t *link_quality_out, int8_t *rssi_out);

bool pbl_bt_analytics_collect_ble_parameters(const struct pbl_bt_device_internal *addr,
                                             struct pbl_bt_le_channel_map *le_chan_map_res);

void pbl_bt_analytics_external_collect_chip_specific_parameters(void);

void pbl_bt_analytics_external_collect_bt_chip_heartbeat(void);

//! Returns true iff there are connection event stats to report
bool pbl_bt_analytics_get_conn_event_stats(struct pbl_bt_slave_conn_event_stats *stats);

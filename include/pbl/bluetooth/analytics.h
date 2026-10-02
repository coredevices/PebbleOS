/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pbl/kernel/compiler.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/conn_event_stats.h>

/**
 * @defgroup bluetooth_analytics Analytics
 * @ingroup bluetooth
 * @brief Link quality and controller statistics for analytics.
 *
 * Not implemented by the NimBLE backend.
 * @{
 */

/** @brief Number of LE data channels. */
#define PBL_BT_NUM_LE_CHANNELS 37

/** @brief LE channel map: one bit per data channel, channel 0 in the LSB of @ref byte0. */
struct PBL_PACKED pbl_bt_le_channel_map {
  /** Channels 0 to 7. */
  uint8_t byte0;
  /** Channels 8 to 15. */
  uint8_t byte1;
  /** Channels 16 to 23. */
  uint8_t byte2;
  /** Channels 24 to 31. */
  uint8_t byte3;
  /** Channels 32 to 36. */
  uint8_t byte4;
};

/**
 * @brief Get the quality of a connection.
 *
 * @param address Peer of the connection.
 * @param[out] link_quality_out Link quality.
 * @param[out] rssi_out Received signal strength in dBm.
 * @return true on success.
 */
bool pbl_bt_analytics_get_connection_quality(const struct pbl_bt_device_internal *address,
                                             uint8_t *link_quality_out, int8_t *rssi_out);

/**
 * @brief Get the channel map of a connection.
 *
 * @param addr Peer of the connection.
 * @param[out] le_chan_map_res Channel map.
 * @return true on success.
 */
bool pbl_bt_analytics_collect_ble_parameters(const struct pbl_bt_device_internal *addr,
                                             struct pbl_bt_le_channel_map *le_chan_map_res);

/** @brief Collect controller specific parameters into the analytics. */
void pbl_bt_analytics_external_collect_chip_specific_parameters(void);

/** @brief Collect the controller heartbeat analytics. */
void pbl_bt_analytics_external_collect_bt_chip_heartbeat(void);

/**
 * @brief Get the connection event statistics.
 *
 * @param[out] stats Statistics.
 * @return true if there are statistics to report.
 */
bool pbl_bt_analytics_get_conn_event_stats(struct pbl_bt_slave_conn_event_stats *stats);

/** @} */

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_conn_event_stats Connection event statistics
 * @ingroup bluetooth
 * @brief Controller statistics of connection events in the peripheral role.
 * @{
 */

/** @brief Connection event counters. */
struct pbl_bt_slave_conn_event_stats {
  /** Connection events that have elapsed. */
  uint32_t num_conn_events;
  /** Connection events the controller never tried to listen for. */
  uint32_t num_conn_events_skipped;
  /** Connection events in which no packet was received from the central. */
  uint32_t num_sync_errors;
  /** Connection events that ended due to a packet type error. */
  uint32_t num_type_errors;
  /** Connection events that ended due to a packet length error. */
  uint32_t num_len_errors;
  /** Connection events that ended due to a packet CRC error. */
  uint32_t num_crc_errors;
  /** Connection events that ended due to a packet MIC error. */
  uint32_t num_mic_errors;
};

/** @} */

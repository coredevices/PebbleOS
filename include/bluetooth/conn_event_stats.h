/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct SlaveConnEventStats {
  uint32_t num_conn_events; // BLE Connection Events that have elapsed
  uint32_t num_conn_events_skipped; // The number of events the controller never tried to listen for
  uint32_t num_sync_errors; // Events where slave did not see a packet from Master
  uint32_t num_type_errors;
  uint32_t num_len_errors;
  uint32_t num_crc_errors; // Events that ended due to a packet CRC error
  uint32_t num_mic_errors; // Events that ended due to a packet MIC error
} SlaveConnEventStats;

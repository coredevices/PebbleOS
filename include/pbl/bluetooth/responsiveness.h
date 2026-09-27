/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <inttypes.h>

#include <pbl/bluetooth/types.h>

#include "pbl/bluetooth/gap_le_connect.h"
#include "pbl/kernel/compiler.h"

enum pbl_bt_consumer {
  PBL_BT_CONSUMER_NONE = 0,
  // Every sub-module has its own consumer name. We try to enter & exit
  // from low latency states within the same module
  PBL_BT_CONSUMER_APP,
  PBL_BT_CONSUMER_LE_PAIRING,
  PBL_BT_CONSUMER_LE_SERVICE_DISCOVERY,
  PBL_BT_CONSUMER_MUSIC_SERVICE_INDEFINITE,
  PBL_BT_CONSUMER_MUSIC_SERVICE_MOMENTARY,
  PBL_BT_CONSUMER_PP_APP_FETCH,
  PBL_BT_CONSUMER_PP_APP_MESSAGE,
  PBL_BT_CONSUMER_PP_AUDIO_ENDPOINT,
  PBL_BT_CONSUMER_PP_GET_BYTES,
  PBL_BT_CONSUMER_PP_LOG_DUMP,
  PBL_BT_CONSUMER_PP_PUT_BYTES,
  PBL_BT_CONSUMER_PP_SCREENSHOT,
  PBL_BT_CONSUMER_PP_VOICE_ENDPOINT,
  PBL_BT_CONSUMER_PROMPT,
  PBL_BT_CONSUMER_TIMELINE_ACTION_MENU,
  PBL_BT_CONSUMER_PRF,
  PBL_BT_CONSUMER_PPS_REMOTE_DEVICE,
  PBL_BT_CONSUMER_UNIT_TESTS, // For unit testing
  PBL_BT_CONSUMER_NUM,
};

enum pbl_bt_response_time_state {
  PBL_BT_RESPONSE_TIME_INVALID = -1,
  PBL_BT_RESPONSE_TIME_MAX = 0, // lowest throughput, most friendly power profile
  PBL_BT_RESPONSE_TIME_MIDDLE,
  PBL_BT_RESPONSE_TIME_MIN, // highest throughput, least friendly power profile
  PBL_BT_RESPONSE_TIME_NUM,
};

//! Callback to call when the requested response time has been negotiated and granted.
typedef void (*pbl_bt_responsiveness_granted_cb_t)(void);

// Longest duration we want to be in Min latency for different modules
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_AUDIO_SECS                (10)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_FETCH_SECS            (5)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS          (10)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_CD_SECS                   (10)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_PROTOCOL_RECV_SECS        (60)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_PUT_BYTES_SECS            (60)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_SCREENSHOT_SECS           (5)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_TIMELINE_ACTION_MENU_SECS (10)
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_VOICE_SECS                (10)

//! Connection Parameters Update Request Packet
struct PBL_PACKED pbl_bt_conn_params_update_req { // PBL_PACKED since this struct is serialized
  uint16_t interval_min_1_25ms;
  uint16_t interval_max_1_25ms;
  uint16_t slave_latency_events;
  uint16_t supervision_timeout_10ms;
};

bool pbl_bt_le_connection_parameter_update(const struct pbl_bt_device_internal *addr,
                                           const struct pbl_bt_conn_params_update_req *req);

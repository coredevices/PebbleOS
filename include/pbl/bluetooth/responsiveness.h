/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <inttypes.h>

#include <pbl/bluetooth/types.h>

#include "pbl/bluetooth/gap_le_connect.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup bluetooth_responsiveness Responsiveness
 * @ingroup bluetooth
 * @brief Connection response time requests and connection parameter updates.
 *
 * Firmware modules request a response time as a consumer; the connection manager
 * (@c comm/bt_conn_mgr.h) picks the fastest one requested and updates the connection
 * parameters with pbl_bt_le_connection_parameter_update().
 * @{
 */

/**
 * @brief Firmware modules that request a response time.
 *
 * Each module enters and exits low latency states under its own consumer.
 */
enum pbl_bt_consumer {
  /** No consumer. */
  PBL_BT_CONSUMER_NONE = 0,
  /** Applications. */
  PBL_BT_CONSUMER_APP,
  /** LE pairing. */
  PBL_BT_CONSUMER_LE_PAIRING,
  /** LE GATT service discovery. */
  PBL_BT_CONSUMER_LE_SERVICE_DISCOVERY,
  /** Music service, until released. */
  PBL_BT_CONSUMER_MUSIC_SERVICE_INDEFINITE,
  /** Music service, briefly. */
  PBL_BT_CONSUMER_MUSIC_SERVICE_MOMENTARY,
  /** Pebble Protocol app fetch. */
  PBL_BT_CONSUMER_PP_APP_FETCH,
  /** Pebble Protocol app messages. */
  PBL_BT_CONSUMER_PP_APP_MESSAGE,
  /** Pebble Protocol audio endpoint. */
  PBL_BT_CONSUMER_PP_AUDIO_ENDPOINT,
  /** Pebble Protocol get bytes. */
  PBL_BT_CONSUMER_PP_GET_BYTES,
  /** Pebble Protocol log dump. */
  PBL_BT_CONSUMER_PP_LOG_DUMP,
  /** Pebble Protocol put bytes. */
  PBL_BT_CONSUMER_PP_PUT_BYTES,
  /** Pebble Protocol screenshot. */
  PBL_BT_CONSUMER_PP_SCREENSHOT,
  /** Pebble Protocol voice endpoint. */
  PBL_BT_CONSUMER_PP_VOICE_ENDPOINT,
  /** Prompt (debug console over Pebble Protocol). */
  PBL_BT_CONSUMER_PROMPT,
  /** Timeline action menu. */
  PBL_BT_CONSUMER_TIMELINE_ACTION_MENU,
  /** Recovery firmware. */
  PBL_BT_CONSUMER_PRF,
  /** Remote device, through the Pebble Pairing Service. */
  PBL_BT_CONSUMER_PPS_REMOTE_DEVICE,
  /** Unit tests. */
  PBL_BT_CONSUMER_UNIT_TESTS,
  /** Number of consumers. */
  PBL_BT_CONSUMER_NUM,
};

/** @brief Connection response time. */
enum pbl_bt_response_time_state {
  /** Invalid. */
  PBL_BT_RESPONSE_TIME_INVALID = -1,
  /** Slowest response, lowest throughput and power. */
  PBL_BT_RESPONSE_TIME_MAX = 0,
  /** Intermediate response time. */
  PBL_BT_RESPONSE_TIME_MIDDLE,
  /** Fastest response, highest throughput and power. */
  PBL_BT_RESPONSE_TIME_MIN,
  /** Number of response times. */
  PBL_BT_RESPONSE_TIME_NUM,
};

/** @brief Called when the requested response time has been negotiated and granted. */
typedef void (*pbl_bt_responsiveness_granted_cb_t)(void);

/**
 * @name Minimum latency timeouts
 * Longest time, in seconds, each module may keep the connection in PBL_BT_RESPONSE_TIME_MIN.
 * @{
 */
/** @brief Audio. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_AUDIO_SECS (10)
/** @brief App fetch. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_FETCH_SECS (5)
/** @brief App messages. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS (10)
/** @brief Core dump transfer. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_CD_SECS (10)
/** @brief Pebble Protocol reception. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_PROTOCOL_RECV_SECS (60)
/** @brief Put bytes. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_PUT_BYTES_SECS (60)
/** @brief Screenshots. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_SCREENSHOT_SECS (5)
/** @brief Timeline action menu. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_TIMELINE_ACTION_MENU_SECS (10)
/** @brief Voice. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_VOICE_SECS (10)
/** @} */

/** @brief Connection parameter update request. Packed because it is serialized. */
struct PBL_PACKED pbl_bt_conn_params_update_req {
  /** Minimum connection interval in 1.25 ms units. */
  uint16_t interval_min_1_25ms;
  /** Maximum connection interval in 1.25 ms units. */
  uint16_t interval_max_1_25ms;
  /** Peripheral latency in connection events. */
  uint16_t slave_latency_events;
  /** Supervision timeout in 10 ms units. */
  uint16_t supervision_timeout_10ms;
};

/**
 * @brief Request new connection parameters.
 *
 * The outcome is reported through pbl_bt_handle_le_conn_params_update_event().
 *
 * @param addr The peer.
 * @param req Requested parameters.
 * @return true if the request was sent.
 */
bool pbl_bt_le_connection_parameter_update(const struct pbl_bt_device_internal *addr,
                                           const struct pbl_bt_conn_params_update_req *req);

/** @} */

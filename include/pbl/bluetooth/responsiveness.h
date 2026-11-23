/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_responsiveness Responsiveness
 * @ingroup bluetooth
 * @brief How fast a connection responds, traded against the power it takes.
 *
 * Consumers ask for a response time on a connection, each for a limited period. The fastest one
 * asked for is negotiated with the central as connection parameters:
 *
 * | Response time | Interval | Peripheral latency | Supervision timeout |
 * | --- | --- | --- | --- |
 * | @ref PBL_BT_RESPONSE_TIME_MAX, @ref PBL_BT_RESPONSE_TIME_MIDDLE | 45 to 60 ms | 4 | 6 s |
 * | @ref PBL_BT_RESPONSE_TIME_MIN | 15 ms | 0 | 6 s |
 *
 * A request the central does not answer is made again, three times at most. Nothing is negotiated
 * before the first request, nor within a second of connecting.
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
/** @brief Put bytes. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_PUT_BYTES_SECS (60)
/** @brief Screenshots. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_SCREENSHOT_SECS (5)
/** @brief Timeline action menu. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_TIMELINE_ACTION_MENU_SECS (10)
/** @brief Voice. */
#define PBL_BT_MIN_LATENCY_MODE_TIMEOUT_VOICE_SECS (10)
/** @} */

/** @brief Period of a request that lasts until it is taken back. */
#define PBL_BT_RESPONSE_TIME_FOREVER UINT16_MAX

/**
 * @brief Ask for a response time on a connection, replacing what the consumer asked for before.
 *
 * Asking for @ref PBL_BT_RESPONSE_TIME_MAX takes the request back, two seconds later so that
 * consecutive operations do not go back and forth.
 *
 * @param conn_handle The connection. Nothing happens if there is none.
 * @param consumer Who asks.
 * @param state The response time.
 * @param max_period_secs How long for, or @ref PBL_BT_RESPONSE_TIME_FOREVER.
 */
void pbl_bt_responsiveness_request(uint16_t conn_handle, enum pbl_bt_consumer consumer,
                                   enum pbl_bt_response_time_state state, uint16_t max_period_secs);

/**
 * @brief Get the response time asked for on a connection.
 *
 * @param conn_handle The connection.
 * @return The fastest response time asked for, @ref PBL_BT_RESPONSE_TIME_MAX if none is.
 */
enum pbl_bt_response_time_state pbl_bt_responsiveness_get(uint16_t conn_handle);

/** @} */

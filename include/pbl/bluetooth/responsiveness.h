/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup bluetooth_responsiveness Responsiveness
 * @ingroup bluetooth
 * @brief How fast a connection responds, traded against the power it takes.
 *
 * Whoever needs a connection fast asks for a response time on it, for a limited period. The fastest
 * one asked for is negotiated with the central as connection parameters:
 *
 * | Response time | Interval | Peripheral latency | Supervision timeout |
 * | --- | --- | --- | --- |
 * | @ref PBL_BT_RESPONSE_TIME_MAX | 45 to 60 ms | 4 | 6 s |
 * | @ref PBL_BT_RESPONSE_TIME_MIN | 15 ms | 0 | 6 s |
 *
 * A request the central does not answer is made again, three times at most. Nothing is negotiated
 * before the first request, nor within a second of connecting.
 * @{
 */

/** @brief Connection response time. */
enum pbl_bt_response_time_state {
  /** Slowest response, lowest throughput and power. */
  PBL_BT_RESPONSE_TIME_MAX = 0,
  /** Fastest response, highest throughput and power. */
  PBL_BT_RESPONSE_TIME_MIN,
  /** Number of response times. */
  PBL_BT_RESPONSE_TIME_NUM,
};

/**
 * @brief A request for a response time.
 *
 * Whoever asks owns one, zero-initialized, that lives for as long as it asks. Its fields are
 * private.
 */
struct pbl_bt_responsiveness_req {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_bt_responsiveness_req *next;
  uint64_t until;
  uint16_t conn_handle;
  uint8_t state;
  bool active;
  /** @endcond */
};

/** @brief Period of a request that lasts until it is taken back. */
#define PBL_BT_RESPONSE_TIME_FOREVER UINT16_MAX

/**
 * @brief Ask for a response time on a connection, replacing what the request asked for before.
 *
 * Asking for @ref PBL_BT_RESPONSE_TIME_MAX takes the request back, two seconds later so that
 * consecutive operations do not go back and forth. A period of zero ends the request at once.
 *
 * @param conn_handle The connection. Nothing happens if there is none.
 * @param req The request of who asks.
 * @param state The response time.
 * @param max_period_secs How long for, or @ref PBL_BT_RESPONSE_TIME_FOREVER. Not used when taking
 * the request back.
 */
void pbl_bt_responsiveness_request(uint16_t conn_handle, struct pbl_bt_responsiveness_req *req,
                                   enum pbl_bt_response_time_state state, uint16_t max_period_secs);

/**
 * @brief Get the response time asked for on a connection.
 *
 * @param conn_handle The connection.
 * @return The fastest response time asked for, @ref PBL_BT_RESPONSE_TIME_MAX if none is.
 */
enum pbl_bt_response_time_state pbl_bt_responsiveness_get(uint16_t conn_handle);

/** @} */

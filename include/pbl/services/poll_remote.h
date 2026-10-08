/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

#include <pbl/services/regular_timer.h>

/**
 * @defgroup services_poll_remote Remote polling
 * @ingroup services
 * @brief Periodic "poll services" requests to the phone app.
 *
 * iOS does not let apps do networking in the background, but a Bluetooth message grants the app
 * a limited time to run. The watch thus periodically asks the phone to poll web services such as
 * email. Intervals are configured per service, usually by the phone (endpoint 0xcafe).
 * @{
 */

/** @brief Service the phone is asked to poll. */
typedef enum {
  /** Email. */
  POLL_REMOTE_SERVICE_MAIL = 0x0,
  /** Data spooling. */
  POLL_REMOTE_SERVICE_DATA_SPOOLING = 0x1,
  /** Number of services. */
  NUM_POLL_REMOTE_SERVICES
} PollRemoteService;

/** @brief Initialize the per-service state. */
void poll_remote_init(void);

/**
 * @brief Send a poll request now, for instance on user interaction.
 *
 * Does nothing if less than the minimum interval elapsed since the last request, or when there
 * is no system session. A sent request restarts the interval count.
 *
 * @param service Service to poll.
 */
void poll_remote_send_request(PollRemoteService service);

/**
 * @brief Start sending poll requests at the configured intervals.
 *
 * Restarts the interval count of every service. Does nothing if already started.
 *
 * @see poll_remote_stop
 */
void poll_remote_start(void);

/**
 * @brief Stop sending poll requests.
 *
 * Does nothing if already stopped.
 *
 * @see poll_remote_start
 */
void poll_remote_stop(void);

/**
 * @brief Set the polling intervals of a service.
 *
 * @param service Service to configure.
 * @param min_interval_minutes Minimum time between two requests; poll_remote_send_request() does
 *        nothing before it elapsed.
 * @param max_interval_minutes Interval of the automatic requests, 0 to disable them.
 */
void poll_remote_set_intervals(PollRemoteService service, const uint8_t min_interval_minutes,
                               const uint8_t max_interval_minutes);

/** @} */

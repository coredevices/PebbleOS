/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file poll_remote.h
//! @brief Subsystem to send a "poll services" message at regular intervals to the iOS app.
//! iOS prevents apps from doing networking in the background. By sending a message over
//! Bluetooth, the app gets a limited time (up to 10 minutes) to run and do networking.
//! In short, the watch is polling the phone to get the phone to poll web services (e.g. email).
//! @author martijn

#pragma once
#include "services/common/regular_timer.h"
#include <stdint.h>

typedef enum {
  POLL_REMOTE_SERVICE_MAIL = 0x0,
  POLL_REMOTE_SERVICE_DATA_SPOOLING = 0x1,
  NUM_POLL_REMOTE_SERVICES
} PollRemoteService;

//! Initializes the PollRemote state
void poll_remote_init(void);

//! Sends poll request to phone app and restarts the timer, unless the time between now
//! and the last time a "poll request" message was sent is shorted than min_interval_minutes.
//! This can be used to trigger the poll for example by user interaction.
void poll_remote_send_request(PollRemoteService service);

//! Starts sending poll requests to the phone app at regular intervals.
//! This will send one request immediately after calling this function.
//! In case polling was already started, this function does nothing.
//! @see poll_remote_stop
void poll_remote_start(void);

//! Stops sending poll requests.
//! In case polling was already stopped, this function does nothing.
//! @see poll_remote_start
void poll_remote_stop(void);

//! Sets the polling intervals.
//! @param min_interval_minutes The minimum interval between two "poll services" requests.
//! Calls to poll_remote_send_request() will be no-ops if min_interval_minutes has not been reached.
//! @param max_interval_minutes The maximum interval between two "poll services" requests.
//! The automatic sending of poll requests will only occur when max_interval_minutes is reached.
void poll_remote_set_intervals(PollRemoteService service,
    const uint8_t min_interval_minutes, const uint8_t max_interval_minutes);

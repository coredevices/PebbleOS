/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "debounced_connection_service.h"

#include "services/common/comm_session/session.h"
#include "services/common/regular_timer.h"
#include "syscall/syscall_internal.h"

//! This module is responsible for propagating debounced connection events.
//! Connection events are passed through right away to subscribers but
//! disconnection events are only passed through if a re-connection did not
//! occur within a small window of time. This way, short disconnect periods
//! can go unnoticed to the end consumer resulting in a better perception of
//! connection reliability
//!
//! At the moment, the connections this module tracks are:
//!   + Watch <-> Mobile App / PebbleKit JS
//!   + Watch <-> third-party App using PebbleKit

typedef enum {
  MobileAppDebounce = 0,
  PebbleKitDebounce,
  NumConnectionsToDebounce,
} DebounceConnection;

static RegularTimerInfo s_debounce_timers[NumConnectionsToDebounce];
static bool s_debounced_state_is_connected[NumConnectionsToDebounce];

static void prv_put_debounced_connection_event(DebounceConnection conn_id) {
  PebbleEvent event = {
    .type = PEBBLE_BT_CONNECTION_DEBOUNCED_EVENT,
    .bluetooth.comm_session_event.is_open = s_debounced_state_is_connected[conn_id],
    .bluetooth.comm_session_event.is_system = (conn_id == MobileAppDebounce),
  };
  event_put(&event);
}

static void prv_handle_disconnection_debounced(void *data) {
  DebounceConnection conn_id = (DebounceConnection)data;
  s_debounced_state_is_connected[conn_id] = false;
  prv_put_debounced_connection_event(conn_id);
  regular_timer_remove_callback(&s_debounce_timers[conn_id]);
}

void debounced_connection_service_init(void) {
  for (int i = 0; i < NumConnectionsToDebounce; i++) {
    s_debounce_timers[i].cb = prv_handle_disconnection_debounced;
    s_debounce_timers[i].cb_data = (void *)(uintptr_t)i;
  }

  // initial state of the connections
  s_debounced_state_is_connected[MobileAppDebounce] = (comm_session_get_system_session() != NULL);
  s_debounced_state_is_connected[PebbleKitDebounce] =
      (comm_session_get_current_app_session() != NULL);
}

DEFINE_SYSCALL(bool, sys_mobile_app_is_connected_debounced, void) {
  return s_debounced_state_is_connected[MobileAppDebounce];
}

DEFINE_SYSCALL(bool, sys_pebblekit_is_connected_debounced, void) {
  return s_debounced_state_is_connected[PebbleKitDebounce];
}

#define DISCONNECT_HIDE_DURATION_SECS 25
void debounced_connection_service_handle_event(PebbleCommSessionEvent *e) {
  DebounceConnection conn_id = e->is_system ? MobileAppDebounce : PebbleKitDebounce;
  bool timer_stopped = false;
  if (!e->is_open) {
    // If we become disconnected don't update apps until we have had a chance
    // to recover the connection. This will make our BT connection seem more
    // reliable.
    regular_timer_add_multisecond_callback(
        &s_debounce_timers[conn_id], DISCONNECT_HIDE_DURATION_SECS);
    return;
  }

  if (regular_timer_is_scheduled(&s_debounce_timers[conn_id])) {
    // we reconnected quickly so no need to notify the app about it
    timer_stopped = regular_timer_remove_callback(&s_debounce_timers[conn_id]);
  }

  if (!timer_stopped) {
    // We've been disconnected long enough that we've already told the app that
    // we disconnected so let the app know that we are connected again.
    s_debounced_state_is_connected[conn_id] = true;
    prv_put_debounced_connection_event(conn_id);
  }
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/kernel/work.h>
#include <pbl/pp/session.h>
#include <pbl/services/app_session.h>
#include <pbl/services/debounced_connection_service.h>

#ifndef CONFIG_RECOVERY_FW
#include <pbl/services/notifications/alerts.h>
#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/services/vibes/vibe_client.h>
#include <pbl/services/vibes/vibe_score.h>
#endif

#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

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

static struct pbl_wallclock_work s_debounce_timers[NumConnectionsToDebounce];
static bool s_debounced_state_is_connected[NumConnectionsToDebounce];

static void prv_put_debounced_connection_event(DebounceConnection conn_id) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_DEBOUNCED_EVENT,
    .bluetooth.comm_session_event.is_open = s_debounced_state_is_connected[conn_id],
    .bluetooth.comm_session_event.is_system = (conn_id == MobileAppDebounce),
  };
  event_put(&event);
}

static void prv_handle_disconnection_debounced(struct pbl_work *work) {
  struct pbl_wallclock_work *timer = container_of(work, struct pbl_wallclock_work, work);
  DebounceConnection conn_id = (DebounceConnection)(timer - s_debounce_timers);

  // First, so that a reconnection handled from now on reports itself.
  pbl_wallclock_work_stop(timer);
  s_debounced_state_is_connected[conn_id] = false;
  prv_put_debounced_connection_event(conn_id);
#ifndef CONFIG_RECOVERY_FW
  if (alerts_should_vibrate_for_type(AlertOther)) {
    uint32_t vibe_id = vibe_score_info_get_resource_id(
        alerts_preferences_get_vibe_score_for_client(VibeClient_OnDisconnect));
    VibeScore *score = vibe_score_create_with_resource_system(0, vibe_id);
    if (score) {
      vibe_score_do_vibe(score);
      vibe_score_destroy(score);
    }
  }
#endif
}

void debounced_connection_service_init(void) {
  for (int i = 0; i < NumConnectionsToDebounce; i++) {
    pbl_wallclock_work_init(&s_debounce_timers[i], prv_handle_disconnection_debounced);
  }

  // initial state of the connections
  s_debounced_state_is_connected[MobileAppDebounce] = (pbl_pp_session_get_system() != nullptr);
  s_debounced_state_is_connected[PebbleKitDebounce] = (app_session_get_current() != nullptr);
}

PBL_SYSCALL(bool, sys_mobile_app_is_connected_debounced) {
  return s_debounced_state_is_connected[MobileAppDebounce];
}

PBL_SYSCALL(bool, sys_pebblekit_is_connected_debounced) {
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
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_debounce_timers[conn_id], 1,
                             DISCONNECT_HIDE_DURATION_SECS);
    return;
  }

  if (pbl_wallclock_work_is_running(&s_debounce_timers[conn_id])) {
    // we reconnected quickly so no need to notify the app about it
    timer_stopped = pbl_wallclock_work_stop(&s_debounce_timers[conn_id]);
  }

  if (!timer_stopped) {
    // We've been disconnected long enough that we've already told the app that
    // we disconnected so let the app know that we are connected again.
    s_debounced_state_is_connected[conn_id] = true;
    prv_put_debounced_connection_event(conn_id);
  }
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/pp/main.h>
#include <pbl/pp/session.h>
#include <pbl/pp/transports/console.h>

#include <kernel/event_loop.h>
#include <kernel/events.h>

void pbl_pp_main_call(void (*fn)(void *data), void *data) {
  launcher_task_add_callback(fn, data);
}

bool pbl_pp_main_is_current(void) {
  return launcher_task_is_current_task();
}

static void prv_put_session_event(bool is_open, bool is_system) {
  PebbleEvent event = {
    .type = PEBBLE_COMM_SESSION_EVENT,
    .bluetooth.comm_session_event.is_open = is_open,
    .bluetooth.comm_session_event.is_system = is_system,
  };

  event_put(&event);
}

static void prv_put_session_events(struct pbl_pp_session *session, bool is_open) {
  enum pbl_pp_destination destination;

  if (pbl_pp_session_get_destination(session, &destination) != 0) {
    return;
  }

  prv_put_session_event(is_open, true);
  if (destination == PBL_PP_DESTINATION_HYBRID) {
    prv_put_session_event(is_open, false);
  }
}

#if defined(CONFIG_PP_TRANSPORT_CONSOLE)
// A console session stands for the connection of a phone.
static void prv_put_connection_event(struct pbl_pp_session *session, bool connected) {
  PebbleEvent event = {
    .type = PEBBLE_BT_CONNECTION_EVENT,
    .bluetooth.connection.state = connected ? PebbleBluetoothConnectionEventStateConnected
                                            : PebbleBluetoothConnectionEventStateDisconnected,
  };

  if (pbl_pp_session_get_transport(session) == &pbl_pp_console_transport) {
    event_put(&event);
  }
}
#endif

static void prv_put_remote_info_event(struct pbl_pp_session *session) {
  PebbleEvent event = {
    .type = PEBBLE_REMOTE_APP_INFO_EVENT,
    .bluetooth.app_info_event.os = pbl_pp_session_get_remote_os(session),
  };

  event_put(&event);
}

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  switch (event) {
    case PBL_PP_SESSION_OPENED:
      prv_put_session_events(session, true);
#if defined(CONFIG_PP_TRANSPORT_CONSOLE)
      prv_put_connection_event(session, true);
#endif
      break;
    case PBL_PP_SESSION_CLOSED:
      prv_put_session_events(session, false);
#if defined(CONFIG_PP_TRANSPORT_CONSOLE)
      prv_put_connection_event(session, false);
#endif
      break;
    case PBL_PP_SESSION_REMOTE_INFO:
      prv_put_remote_info_event(session);
      break;
    default:
      break;
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(events, prv_session_cb, nullptr);

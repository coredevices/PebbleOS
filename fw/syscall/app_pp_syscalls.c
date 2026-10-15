/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/services/comm_session/app_session_capabilities.h>
#include <pbl/services/comm_session/session.h>

#include <applib/app_message/app_message_internal.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>
#include <syscall/syscall_define.h>

static bool prv_is_endpoint_allowed(uint16_t endpoint_id) {
  return (endpoint_id == APP_MESSAGE_ENDPOINT_ID);
}

PBL_SYSCALL(CommSession *, sys_app_pp_get_comm_session) {
  CommSession *app_session = nullptr;
  comm_session_sanitize_app_session(&app_session);
  return app_session;
}

PBL_SYSCALL(bool, sys_app_pp_send_data, (VAL, CommSession *, session), (VAL, uint16_t, endpoint_id),
            (IN_BUF, const uint8_t *, data, length), (VAL, uint16_t, length)) {
  if (from_user && pebble_task_get_current() != PebbleTask_App) {
    syscall_failed();
  }
  if (!prv_is_endpoint_allowed(endpoint_id)) {
    syscall_failed();
  }

  comm_session_sanitize_app_session(&session);
  if (!session) {
    // No session connected that can serve the currently running app
    return false;
  }

  const AppInstallId app_id = app_manager_get_current_app_id();
  app_install_mark_prioritized(app_id, true /* can_expire */);

  // TODO: apply some heuristic to decide whether to put connection in fast mode or not:
  // https://pebbletechnology.atlassian.net/browse/PBL-21538
  comm_session_set_responsiveness(session, PBL_BT_CONSUMER_PP_APP_MESSAGE, PBL_BT_RESPONSE_TIME_MIN,
                                  PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS);

  // FIXME: Let the app task wait indefinitely for now
  const uint32_t timeout_ms = ~0;
  return comm_session_send_data(session, endpoint_id, data, length, timeout_ms);
}

PBL_SYSCALL(bool, sys_app_pp_has_capability, (VAL, CommSessionCapability, capability)) {
  return comm_session_current_app_session_cache_has_capability(capability);
}
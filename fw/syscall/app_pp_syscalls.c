/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/app_session.h>

#include <applib/app_comm.h>
#include <applib/app_message/app_message_internal.h>
#include <comm/bt_conn_mgr.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>

static bool prv_is_endpoint_allowed(uint16_t endpoint_id) {
  return (endpoint_id == APP_MESSAGE_ENDPOINT_ID);
}

PBL_SYSCALL(struct pbl_pp_session *, sys_app_pp_get_comm_session) {
  struct pbl_pp_session *app_session = nullptr;
  app_session_sanitize(&app_session);
  return app_session;
}

PBL_SYSCALL(bool, sys_app_pp_send_data, (VAL, struct pbl_pp_session *, session),
            (VAL, uint16_t, endpoint_id), (IN_BUF, const uint8_t *, data, length),
            (VAL, uint16_t, length)) {
  if (from_user && pebble_task_get_current() != PebbleTask_App) {
    pbl_syscall_fail();
  }
  if (!prv_is_endpoint_allowed(endpoint_id)) {
    pbl_syscall_fail();
  }

  app_session_sanitize(&session);
  if (!session) {
    // No session connected that can serve the currently running app
    return false;
  }

  const AppInstallId app_id = app_manager_get_current_app_id();
  app_install_mark_prioritized(app_id, true /* can_expire */);

  // TODO: use a heuristic to decide whether to put the connection in fast mode
  pbl_pp_session_set_responsiveness(session, PBL_BT_CONSUMER_PP_APP_MESSAGE,
                                    PBL_BT_RESPONSE_TIME_MIN,
                                    PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS, nullptr);

  // FIXME: Let the app task wait indefinitely for now
  const uint32_t timeout_ms = ~0;
  return pbl_pp_send(session, endpoint_id, data, length, timeout_ms) == 0;
}

PBL_SYSCALL(bool, sys_app_pp_has_capability, (VAL, enum pbl_pp_capability, capability)) {
  return app_session_has_capability(capability);
}

PBL_SYSCALL(void, sys_app_comm_set_responsiveness, (VAL, SniffInterval, interval)) {
  struct pbl_pp_session *session = app_session_get_current();

  switch (interval) {
    case SNIFF_INTERVAL_REDUCED:
      pbl_pp_session_set_responsiveness(session, PBL_BT_CONSUMER_APP, PBL_BT_RESPONSE_TIME_MIDDLE,
                                        MAX_PERIOD_RUN_FOREVER, nullptr);
      return;
    case SNIFF_INTERVAL_NORMAL:
      pbl_pp_session_set_responsiveness(session, PBL_BT_CONSUMER_APP, PBL_BT_RESPONSE_TIME_MAX, 0,
                                        nullptr);
      return;
  }
  PBL_LOG_WRN("Invalid sniff interval");
  pbl_syscall_fail();
}

PBL_SYSCALL(bool, sys_system_pp_has_capability, (VAL, enum pbl_pp_capability, capability)) {
  return pbl_pp_session_has_capability(pbl_pp_session_get_system(), capability);
}

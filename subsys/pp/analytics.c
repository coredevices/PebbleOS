/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/analytics/analytics.h>

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  if (!pbl_pp_session_is_system(session)) {
    return;
  }

  switch (event) {
    case PBL_PP_SESSION_OPENED:
      PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
      PBL_ANALYTICS_TIMER_START(connectivity_connected_time_ms);
      break;
    case PBL_PP_SESSION_CLOSED:
      PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
      PBL_ANALYTICS_TIMER_STOP(connectivity_connected_time_ms);
      break;
    default:
      break;
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(analytics, prv_session_cb, nullptr);

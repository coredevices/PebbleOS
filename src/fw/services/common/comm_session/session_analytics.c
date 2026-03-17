/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "session_analytics.h"

#include "drivers/rtc.h"

#if MEMFAULT
#include "memfault/metrics/connectivity.h"
#endif
#include "services/common/comm_session/session_internal.h"
#include "services/common/analytics/analytics.h"
#include "services/common/ping.h"
#include "util/time/time.h"

CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session) {
  return session->transport_imp->get_type(session->transport);
}

void comm_session_analytics_open_session(CommSession *session) {
  const bool is_system = (session->destination != TransportDestinationApp);
  if (is_system) {
#if MEMFAULT
    memfault_metrics_connectivity_connected_state_change(
      kMemfaultMetricsConnectivityState_Connected);
#endif
  }
  session->open_ticks = rtc_get_ticks();
}

void comm_session_analytics_close_session(CommSession *session, CommSessionCloseReason reason) {
  const bool is_system = (session->destination != TransportDestinationApp);
  if (is_system) {
#if MEMFAULT
    memfault_metrics_connectivity_connected_state_change(
      kMemfaultMetricsConnectivityState_ConnectionLost);
#endif
  }

  const RtcTicks duration_ticks = (rtc_get_ticks() - session->open_ticks);
  const uint16_t duration_mins = ((duration_ticks / RTC_TICKS_HZ) / SECONDS_PER_MINUTE);
  const Uuid *optional_app_uuid = comm_session_get_uuid(session);

}
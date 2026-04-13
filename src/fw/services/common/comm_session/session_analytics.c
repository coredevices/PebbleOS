/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "session_analytics.h"

#include "drivers/rtc.h"

#if MEMFAULT
#include "memfault/metrics/connectivity.h"
#include "memfault_chunk_collector.h"
#endif
#include "services/common/comm_session/session_internal.h"
#include "services/common/analytics/analytics.h"
#if defined(ANALYTICS_DIRECT_HEARTBEAT)
#include "services/common/analytics/direct/heartbeat_v70.h"
#endif
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
    // Trigger a delayed Memfault chunk collection so any pending coredump data
    // gets pushed into datalogging shortly after the phone connects, rather than
    // waiting for the 15-minute periodic timer. The delay gives the phone time
    // to complete the datalogging Report handshake.
    memfault_chunk_collect_after_delay();
#endif
#if defined(ANALYTICS_DIRECT_HEARTBEAT)
    v70_connectivity_connected();
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
#if defined(ANALYTICS_DIRECT_HEARTBEAT)
    v70_connectivity_disconnected();
#endif
  }

}
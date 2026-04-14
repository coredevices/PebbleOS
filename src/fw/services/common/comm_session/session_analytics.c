/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/common/comm_session/session_analytics.h"

#include "drivers/rtc.h"

#if MEMFAULT
#include "memfault_chunk_collector.h"
#endif
#include "pbl/services/common/comm_session/session_internal.h"
#include "pbl/services/common/analytics/analytics.h"
#include "pbl/services/common/ping.h"
#include "util/time/time.h"

CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session) {
  return session->transport_imp->get_type(session->transport);
}

void comm_session_analytics_open_session(CommSession *session) {
  const bool is_system = (session->destination != TransportDestinationApp);
  if (is_system) {
    PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
    PBL_ANALYTICS_TIMER_START(connectivity_connected_time_ms);
#if MEMFAULT
    // Trigger a delayed Memfault chunk collection so any pending coredump data
    // gets pushed into datalogging shortly after the phone connects, rather than
    // waiting for the 15-minute periodic timer. The delay gives the phone time
    // to complete the datalogging Report handshake.
    memfault_chunk_collect_after_delay();
#endif
  }
  session->open_ticks = rtc_get_ticks();
}

void comm_session_analytics_close_session(CommSession *session, CommSessionCloseReason reason) {
  const bool is_system = (session->destination != TransportDestinationApp);
  if (is_system) {
    PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
    PBL_ANALYTICS_TIMER_STOP(connectivity_connected_time_ms);
  }

}
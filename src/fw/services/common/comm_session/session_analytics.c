/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "session_analytics.h"

#include "drivers/rtc.h"

#if MEMFAULT
#include "memfault/metrics/connectivity.h"
#endif
#include "services/common/comm_session/session_internal.h"
#include "services/common/analytics/analytics.h"
#include "services/common/ping.h"
#include "util/time/time.h"

//! returns the analytic timer id we want to use
static int prv_get_analytic_id_for_session(CommSession *session) {
  if (comm_session_analytics_get_transport_type(session) == CommSessionTransportType_PPoGATT) {
    return ANALYTICS_DEVICE_METRIC_BT_PEBBLE_PPOGATT_APP_TIME;
  } else {
    return ANALYTICS_DEVICE_METRIC_BT_PEBBLE_SPP_APP_TIME;
  }
}

CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session) {
  return session->transport_imp->get_type(session->transport);
}

void comm_session_analytics_open_session(CommSession *session) {
  const bool is_system = (session->destination != TransportDestinationApp);
  if (is_system) {
    int analytic_id = prv_get_analytic_id_for_session(session);
    analytics_stopwatch_start(analytic_id, AnalyticsClient_System);
    analytics_inc(ANALYTICS_DEVICE_METRIC_BT_SYSTEM_SESSION_OPEN_COUNT, AnalyticsClient_System);
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
    int analytic_id = prv_get_analytic_id_for_session(session);
    analytics_stopwatch_stop(analytic_id);
#if MEMFAULT
    memfault_metrics_connectivity_connected_state_change(
      kMemfaultMetricsConnectivityState_ConnectionLost);
#endif
  }

  const RtcTicks duration_ticks = (rtc_get_ticks() - session->open_ticks);
  const uint16_t duration_mins = ((duration_ticks / RTC_TICKS_HZ) / SECONDS_PER_MINUTE);
  const Uuid *optional_app_uuid = comm_session_get_uuid(session);

  analytics_event_session_close(is_system, optional_app_uuid, reason, duration_mins);
}

//! Increment "bytes sent" counter and perform app ping if due
void comm_session_analytics_inc_bytes_sent(CommSession *session, uint16_t length) {
  CommSessionType type = comm_session_get_type(session);
  AnalyticsMetric metric;
  switch (type) {
    case CommSessionTypeSystem:
      metric = ANALYTICS_DEVICE_METRIC_BT_PRIVATE_BYTE_OUT_COUNT;

      // We know that bluetooth is already active. If we just sent a message to the Pebble mobile
      // app, this is a good time to see if we should send our ping out to it as well.
      ping_send_if_due();
      break;

    case CommSessionTypeApp:
      metric = ANALYTICS_DEVICE_METRIC_BT_PUBLIC_BYTE_OUT_COUNT;
      break;

    case CommSessionTypeInvalid:
    default:
      return;
  }
  analytics_add(metric, length, AnalyticsClient_System);
}

void comm_session_analytics_inc_bytes_received(CommSession *session, uint16_t length) {
  const AnalyticsMetric metric = (comm_session_get_type(session) == CommSessionTypeSystem) ?
                                                ANALYTICS_DEVICE_METRIC_BT_PRIVATE_BYTE_IN_COUNT :
                                                ANALYTICS_DEVICE_METRIC_BT_PUBLIC_BYTE_IN_COUNT;
  analytics_add(metric, length, AnalyticsClient_System);
}

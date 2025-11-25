/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/analytics/analytics.h"

//! Stub for PRF

void analytics_init(void) {
}

void analytics_set(AnalyticsMetric metric, int64_t value, AnalyticsClient client) {
}

void analytics_max(AnalyticsMetric metric, int64_t val, AnalyticsClient client) {
}

void analytics_inc(AnalyticsMetric metric, AnalyticsClient client) {
}

void analytics_add(AnalyticsMetric metric, int64_t amount, AnalyticsClient client) {
}

void analytics_stopwatch_start(AnalyticsMetric metric, AnalyticsClient client) {
}

void analytics_stopwatch_start_at_rate(AnalyticsMetric metric,
                                       uint32_t count_per_sec,
                                       AnalyticsClient client) {
}

void analytics_stopwatch_stop(AnalyticsMetric metric) {
}

void analytics_event_app_oom(AnalyticsEvent type,
                             uint32_t requested_size, uint32_t total_size,
                             uint32_t total_free, uint32_t largest_free_block) {
}

void analytics_event_app_launch(const Uuid *uuid) {
}

void analytics_event_bt_connection_or_disconnection(AnalyticsEvent type, uint8_t reason) {
}

void analytics_event_bt_error(AnalyticsEvent type, uint32_t error) {
}

void analytics_event_bt_cc2564x_lockup_error(void) {
}

void analytics_event_bt_app_launch_error(uint8_t gatt_error) {
}

void analytics_event_session_close(bool is_system_session, const Uuid *optional_app_uuid,
                                   CommSessionCloseReason reason, uint16_t session_duration_mins) {
}

void analytics_event_bt_le_disconnection(uint8_t reason, uint8_t remote_bt_version,
                                         uint16_t remote_bt_company_id,
                                         uint16_t remote_bt_subversion) {
}

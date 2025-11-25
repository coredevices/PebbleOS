/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/analytics/analytics.h"
#include "services/common/analytics/analytics_event.h"

//! Stub for PRF

void sys_analytics_set(AnalyticsMetric metric, uint64_t value, AnalyticsClient client) {
}

void sys_analytics_add(AnalyticsMetric metric, uint64_t increment, AnalyticsClient client) {
}

void sys_analytics_inc(AnalyticsMetric metric, AnalyticsClient client) {
}

void sys_analytics_max(AnalyticsMetric metric, int64_t val, AnalyticsClient client) {
}

void sys_analytics_logging_log_event(AnalyticsEventBlob *event_blob) {
}

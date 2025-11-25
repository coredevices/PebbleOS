/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/analytics/analytics.h"

void analytics_init(void) {}

void analytics_set(AnalyticsMetric metric, int64_t val, AnalyticsClient client) {}
void analytics_set_for_uuid(AnalyticsMetric metric, int64_t val, const Uuid *uuid) {}

// TODO: Remove this, and add analytics_append_array or something. See PBL-5333
void analytics_set_entire_array(AnalyticsMetric metric, const void *data, AnalyticsClient client) {}

void analytics_inc(AnalyticsMetric metric, AnalyticsClient client) {}
void analytics_inc_for_uuid(AnalyticsMetric metric, const Uuid *uuid) {}

void analytics_add(AnalyticsMetric metric, int64_t amount, AnalyticsClient client) {}
void analytics_add_for_uuid(AnalyticsMetric metric, int64_t amount, const Uuid *uuid) {}

void analytics_stopwatch_start(AnalyticsMetric metric, AnalyticsClient client) {}
void analytics_stopwatch_stop(AnalyticsMetric metric) {}

void analytics_stopwatch_start_at_rate(AnalyticsMetric metric, uint32_t count_per_sec, AnalyticsClient client) {}

void analytics_event_app_launch(const Uuid *uuid) {}
void analytics_event_bt_error(AnalyticsEvent type, uint32_t error) {}
void analytics_event_pin_created(time_t timestamp, const Uuid *parent_id) {}
void analytics_event_pin_updated(time_t timestamp, const Uuid *parent_id) {}

void analytics_event_pin_open(time_t timestamp, const Uuid *parent_id) {}

void analytics_event_pin_action(time_t timestamp, const Uuid *parent_id,
                                TimelineItemActionType action_type) {}

void analytics_event_pin_app_launch(time_t timestamp, const Uuid *parent_id) {}

void analytics_event_canned_response(const char *response, bool successfully_sent) {}

void analytics_event_health_insight_created(time_t timestamp,
                                            ActivityInsightType insight_type,
                                            PercentTier pct_tier) {}
void analytics_event_health_insight_response(time_t timestamp, ActivityInsightType insight_type,
                                             ActivitySessionType activity_type,
                                             ActivityInsightResponseType response_id) {}

void analytics_event_alarm(AnalyticsEvent event_type, const AlarmInfo *info) {}

void analytics_event_PPoGATT_disconnect(time_t timestamp, bool successful_reconnect) {}

void analytics_event_ble_hrm(BleHrmEventSubtype subtype) {}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/health_service.h"

bool health_service_private_get_metric_history(HealthMetric metric, uint32_t history_len,
                                               int32_t *history) {
  return false;
}

bool health_service_private_non_zero_filter(int index, int32_t value, void *tm_weekday_ref) {
  return false;
}

bool health_service_private_weekday_filter(int index, int32_t value, void *tm_weekday_ref) {
  return false;
}

bool health_service_private_weekend_filter(int index, int32_t value, void *tm_weekday_ref) {
  return false;
}

HealthValue health_service_sum_today(HealthMetric metric) {
  return 0;
}

HealthValue health_service_peek_current_value(HealthMetric metric) {
  return 0;
}

bool health_service_set_heart_rate_sample_period(uint16_t interval_sec) {
  return false;
}

bool health_service_events_subscribe(HealthEventHandler handler, void *context) {
  return false;
}

bool health_service_events_unsubscribe(void) {
  return false;
}

HealthValue health_service_sum_averaged(HealthMetric metric, time_t time_start, time_t time_end,
                                        HealthServiceTimeScope scope) {
  return 0;
}

uint32_t health_service_get_minute_history(HealthMinuteData *minute_data, uint32_t max_records,
                                           time_t *time_start, time_t *time_end) {
  return 0;
}

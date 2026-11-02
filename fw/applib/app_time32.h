/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <pbl/services/clock.h>
#include <pbl/util/time.h>

#include <applib/alarm_service.h>
#include <applib/app_glance.h>
#include <applib/app_wakeup.h>
#include <applib/health_service.h>
#include <applib/pbl_std/pbl_std.h>
#include <syscall/syscall.h>

// The app ABI of apps built with a 32-bit time_t: legacy platforms, and every app built before
// the SDK moved to a 64-bit one. Their jump table slots point at the *_time32 functions, which
// take and return the 32-bit time those apps use.

//! Widen a time an app passed in, relative to the current time.
static inline time_t app_time_from_time32(pbl_time32_t t) {
  return pbl_time_from_time32(t, sys_get_time());
}

//! Narrow a time handed back to an app, or return -1 if the app could not read it back.
static inline pbl_time32_t app_time_to_time32_or_error(time_t t) {
  const pbl_time32_t t32 = pbl_time_to_time32(t);
  if (pbl_time_from_time32(t32, sys_get_time()) != t) {
    return -1;
  }
  return t32;
}

//! AppGlanceSlice with a 32-bit time.
typedef struct AppGlanceSlice32 {
  AppGlanceSliceLayout layout;
  pbl_time32_t expiration_time;
} AppGlanceSlice32;

//! HealthActivityIteratorCB with 32-bit times.
typedef bool (*HealthActivityIteratorCB32)(HealthActivity activity, pbl_time32_t time_start,
                                           pbl_time32_t time_end, void *context);

pbl_time32_t pbl_override_time_time32(pbl_time32_t *tloc);
uint16_t time_ms_time32(pbl_time32_t *tloc, uint16_t *out_ms);
double pbl_override_difftime_time32(pbl_time32_t end, pbl_time32_t beginning);
pbl_time32_t pbl_override_mktime_time32(struct pbl_tm *tb);
struct pbl_tm *pbl_override_gmtime_time32(const pbl_time32_t *timep);
struct pbl_tm *pbl_override_localtime_time32(const pbl_time32_t *timep);
pbl_time32_t time_start_of_today_time32(void);
pbl_time32_t clock_to_timestamp_time32(WeekDay day, int hour, int minute);

WakeupId app_wakeup_schedule_time32(pbl_time32_t timestamp, int32_t cookie, bool notify_if_missed);
bool app_wakeup_query_time32(WakeupId wakeup_id, pbl_time32_t *timestamp);

bool alarm_service_peek_next_time32(pbl_time32_t *timestamp_out);

AppGlanceResult app_glance_add_slice_time32(AppGlanceReloadSession *session,
                                            AppGlanceSlice32 slice);

HealthValue health_service_sum_time32(HealthMetric metric, pbl_time32_t time_start,
                                      pbl_time32_t time_end);
HealthValue health_service_sum_averaged_time32(HealthMetric metric, pbl_time32_t time_start,
                                               pbl_time32_t time_end, HealthServiceTimeScope scope);
HealthValue health_service_aggregate_averaged_time32(HealthMetric metric, pbl_time32_t time_start,
                                                     pbl_time32_t time_end,
                                                     HealthAggregation aggregation,
                                                     HealthServiceTimeScope scope);
void health_service_activities_iterate_time32(HealthActivityMask activity_mask,
                                              pbl_time32_t time_start, pbl_time32_t time_end,
                                              HealthIterationDirection direction,
                                              HealthActivityIteratorCB32 callback, void *context);
HealthServiceAccessibilityMask health_service_metric_accessible_time32(HealthMetric metric,
                                                                       pbl_time32_t time_start,
                                                                       pbl_time32_t time_end);
HealthServiceAccessibilityMask health_service_metric_averaged_accessible_time32(
    HealthMetric metric, pbl_time32_t time_start, pbl_time32_t time_end,
    HealthServiceTimeScope scope);
HealthServiceAccessibilityMask health_service_metric_aggregate_averaged_accessible_time32(
    HealthMetric metric, pbl_time32_t time_start, pbl_time32_t time_end,
    HealthAggregation aggregation, HealthServiceTimeScope scope);
HealthServiceAccessibilityMask health_service_any_activity_accessible_time32(
    HealthActivityMask activity_mask, pbl_time32_t time_start, pbl_time32_t time_end);
uint32_t health_service_get_minute_history_time32(HealthMinuteData *minute_data,
                                                  uint32_t max_records, pbl_time32_t *time_start,
                                                  pbl_time32_t *time_end);

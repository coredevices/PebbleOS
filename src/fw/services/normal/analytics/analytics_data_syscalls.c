/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/analytics/analytics.h"
#include "services/common/analytics/analytics_event.h"
#include "services/common/analytics/analytics_logging.h"
#include "syscall/syscall_internal.h"

DEFINE_SYSCALL(void, sys_analytics_set, AnalyticsMetric metric, uint64_t value,
                                        AnalyticsClient client) {
  analytics_set(metric, value, client);
}

DEFINE_SYSCALL(void, sys_analytics_set_entire_array, AnalyticsMetric metric, const void *value,
                                                     AnalyticsClient client) {
  analytics_set_entire_array(metric, value, client);
}

DEFINE_SYSCALL(void, sys_analytics_add, AnalyticsMetric metric, uint64_t increment,
                                        AnalyticsClient client) {
  analytics_add(metric, increment, client);
}

DEFINE_SYSCALL(void, sys_analytics_inc, AnalyticsMetric metric, AnalyticsClient client) {
  analytics_inc(metric, client);
}

DEFINE_SYSCALL(void, sys_analytics_stopwatch_start, AnalyticsMetric metric,
                                                    AnalyticsClient client) {
  analytics_stopwatch_start(metric, client);
}

DEFINE_SYSCALL(void, sys_analytics_stopwatch_stop, AnalyticsMetric metric) {
  analytics_stopwatch_stop(metric);
}

static bool prv_is_event_allowed(const AnalyticsEventBlob *const event_blob) {
  switch (event_blob->event) {
    case AnalyticsEvent_AppOOMNative:
    case AnalyticsEvent_AppOOMRocky:
      return true;

    default:
      // Don't allow any other event types:
      return false;
  }
}

DEFINE_SYSCALL(void, sys_analytics_logging_log_event, AnalyticsEventBlob *event_blob) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(event_blob, sizeof(*event_blob));
  }
  if (!prv_is_event_allowed(event_blob)) {
    syscall_failed();
  }
  analytics_logging_log_event(event_blob);
}

DEFINE_SYSCALL(void, sys_analytics_max, AnalyticsMetric metric, int64_t val,
               AnalyticsClient client) {
  analytics_max(metric, val, client);
}

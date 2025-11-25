/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "jmem-heap.h"

static int s_app_heap_analytics_log_stats_to_app_heartbeat_call_count;
void app_heap_analytics_log_stats_to_app_heartbeat(bool is_rocky_app) {
  s_app_heap_analytics_log_stats_to_app_heartbeat_call_count++;
}

static int s_app_heap_analytics_log_rocky_heap_oom_fault_call_count;
void app_heap_analytics_log_rocky_heap_oom_fault(void) {
  s_app_heap_analytics_log_rocky_heap_oom_fault_call_count++;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_heap_analytics.h"

#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "services/common/analytics/analytics_event.h"
#include "syscall/syscall.h"

#include <util/heap.h>

static bool prv_is_current_task_app_or_worker(void) {
  switch (pebble_task_get_current()) {
    case PebbleTask_App:
    case PebbleTask_Worker:
      return true;
    default:
      return false;
  }
}

void app_heap_analytics_log_native_heap_oom_fault(size_t requested_size, Heap *heap) {
  if (!prv_is_current_task_app_or_worker()) {
    return;
  }
  const uint32_t total_size = heap_size(heap);
  unsigned int used = 0;
  unsigned int total_free = 0;
  unsigned int largest_free_block = 0;
  heap_calc_totals(heap, &used, &total_free, &largest_free_block);
  analytics_event_app_oom(AnalyticsEvent_AppOOMNative, requested_size,
                          total_size, total_free, largest_free_block);
}

void app_heap_analytics_log_stats_to_app_heartbeat(void) {
  Heap *const heap = task_heap_get_for_current_task();
  sys_analytics_max(ANALYTICS_APP_METRIC_MEM_NATIVE_HEAP_SIZE, heap_size(heap),
                    AnalyticsClient_CurrentTask);
  sys_analytics_max(ANALYTICS_APP_METRIC_MEM_NATIVE_HEAP_PEAK, heap->high_water_mark,
                    AnalyticsClient_CurrentTask);
}

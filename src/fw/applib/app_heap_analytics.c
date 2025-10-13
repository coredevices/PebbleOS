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

#include "app_heap_analytics.h"

#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "services/common/analytics/analytics_event.h"
#include "syscall/syscall.h"

#if CAPABILITY_HAS_ROCKY_JS && !RECOVERY_FW
#include "jmem-heap.h"
#endif

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

void app_heap_analytics_log_rocky_heap_oom_fault(void) {
#if CAPABILITY_HAS_ROCKY_JS && !RECOVERY_FW
  if (!prv_is_current_task_app_or_worker()) {
    return;
  }

  jmem_heap_stats_t jerry_mem_stats = {};
  jmem_heap_get_stats(&jerry_mem_stats);
  const size_t requested_size = 0; // not available unfortunately
  const size_t free = (jerry_mem_stats.size - jerry_mem_stats.allocated_bytes);
  analytics_event_app_oom(AnalyticsEvent_AppOOMRocky, requested_size,
                          jerry_mem_stats.size, free, jerry_mem_stats.largest_free_block_bytes);
#endif
}

void app_heap_analytics_log_stats_to_app_heartbeat(bool is_rocky_app) {
  Heap *const heap = task_heap_get_for_current_task();
  sys_analytics_max(ANALYTICS_APP_METRIC_MEM_NATIVE_HEAP_SIZE, heap_size(heap),
                    AnalyticsClient_CurrentTask);
  sys_analytics_max(ANALYTICS_APP_METRIC_MEM_NATIVE_HEAP_PEAK, heap->high_water_mark,
                    AnalyticsClient_CurrentTask);
#if CAPABILITY_HAS_ROCKY_JS && !RECOVERY_FW

  if (is_rocky_app) {
    jmem_heap_stats_t jerry_mem_stats = {};
    jmem_heap_get_stats(&jerry_mem_stats);
    sys_analytics_max(ANALYTICS_APP_METRIC_MEM_ROCKY_HEAP_PEAK,
                      jerry_mem_stats.global_peak_allocated_bytes,
                      AnalyticsClient_CurrentTask);
    sys_analytics_max(ANALYTICS_APP_METRIC_MEM_ROCKY_HEAP_WASTE,
                      jerry_mem_stats.global_peak_waste_bytes,
                      AnalyticsClient_CurrentTask);
  }
#endif
}

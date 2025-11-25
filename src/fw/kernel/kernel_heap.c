/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/task_watchdog.h"
#include "kernel_heap.h"
#include "mcu/interrupts.h"
#include "services/common/analytics/analytics.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

static Heap s_kernel_heap;
static bool s_interrupts_disabled_by_heap;
static uint32_t s_pri_mask; // cache basepri mask we restore to in heap_unlock

// Locking callbacks for our kernel heap.
// FIXME: Note that we use __set_BASEPRI() instead of a mutex because our heap
// has to be used before we even initialize FreeRTOS. We don't use
// __disable_irq() because we want to catch any hangs in the heap code with our
// high priority watchdog so that a coredump is triggered.

static void prv_heap_lock(void *ctx) {
  if (mcu_state_are_interrupts_enabled()) {
    s_pri_mask = __get_BASEPRI();
    __set_BASEPRI((TASK_WATCHDOG_PRIORITY + 1) << (8 - __NVIC_PRIO_BITS));
    s_interrupts_disabled_by_heap = true;
  }
}

static void prv_heap_unlock(void *ctx) {
  if (s_interrupts_disabled_by_heap) {
    __set_BASEPRI(s_pri_mask);
    s_interrupts_disabled_by_heap = false;
  }
}

void kernel_heap_init(void) {
  extern int _heap_start;
  extern int _heap_end;

  heap_init(&s_kernel_heap, &_heap_start, &_heap_end, true);
  heap_set_lock_impl(&s_kernel_heap, (HeapLockImpl) {
    .lock_function = prv_heap_lock,
    .unlock_function = prv_heap_unlock
  });
}

void analytics_external_collect_kernel_heap_stats(void) {
  uint32_t headroom = heap_get_minimum_headroom(&s_kernel_heap);
  // Reset the high water mark so we can see if there are certain periods of time
  // where we really tax the heap
  s_kernel_heap.high_water_mark = s_kernel_heap.current_size;
  analytics_set(ANALYTICS_DEVICE_METRIC_KERNEL_HEAP_MIN_HEADROOM_BYTES, headroom,
                AnalyticsClient_System);
}

Heap* kernel_heap_get(void) {
  return &s_kernel_heap;
}

// Serial Commands
///////////////////////////////////////////////////////////
#ifdef MALLOC_INSTRUMENTATION
void command_dump_malloc_kernel(void) {
  heap_dump_malloc_instrumentation_to_dbgserial(&s_kernel_heap);
}
#endif

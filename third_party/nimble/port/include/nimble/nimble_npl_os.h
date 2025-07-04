/*
 * Copyright 2025 Google LLC
 * Copyright 2015-2024 The Apache Software Foundation
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

// This is derived from the freertos port provided by NimBLE
// and modified to suit Pebble OS (timers, mutexes).

#ifndef _NIMBLE_NPL_OS_H_
#define _NIMBLE_NPL_OS_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "kernel/pbl_malloc.h"
#include "os/mutex.h"
#include "queue.h"
#include "semphr.h"
#include "services/common/new_timer/new_timer.h"
#include "task.h"
#include "timers.h"

#include "os/os_cputime.h"

#if NRF52_SERIES
#include "drivers/nrf5/hfxo.h"
#endif

#define BLE_NPL_OS_ALIGNMENT 4

#define BLE_NPL_TIME_FOREVER portMAX_DELAY

typedef uint32_t ble_npl_time_t;
typedef int32_t ble_npl_stime_t;

struct ble_npl_event {
  bool queued;
  ble_npl_event_fn *fn;
  void *arg;
};

struct ble_npl_eventq {
  QueueHandle_t q;
};

struct ble_npl_callout {
#if configUSE_TIMERS
  TimerHandle_t handle;
#else
  TimerID handle;
#endif
  struct ble_npl_eventq *evq;
  struct ble_npl_event ev;
  uint64_t ticks;
};

struct ble_npl_mutex {
  PebbleRecursiveMutex *handle;
};

struct ble_npl_sem {
  SemaphoreHandle_t handle;
};

#include "npl_pebble.h"

static inline bool ble_npl_os_started(void) {
  return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

static inline void *ble_npl_get_current_task_id(void) { return xTaskGetCurrentTaskHandle(); }

static inline void ble_npl_eventq_init(struct ble_npl_eventq *evq) {
  evq->q = xQueueCreate(32, sizeof(struct ble_npl_eventq *));
}

static inline struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *evq,
                                                       ble_npl_time_t tmo) {
  return npl_pebble_eventq_get(evq, tmo);
}

static inline void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  npl_pebble_eventq_put(evq, ev);
}

static inline void ble_npl_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  npl_pebble_eventq_remove(evq, ev);
}

static inline void ble_npl_event_run(struct ble_npl_event *ev) { ev->fn(ev); }

static inline bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq) {
  return xQueueIsQueueEmptyFromISR(evq->q);
}

static inline void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg) {
  memset(ev, 0, sizeof(*ev));
  ev->fn = fn;
  ev->arg = arg;
}

static inline bool ble_npl_event_is_queued(struct ble_npl_event *ev) { return ev->queued; }

static inline void *ble_npl_event_get_arg(struct ble_npl_event *ev) { return ev->arg; }

static inline void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg) { ev->arg = arg; }

static inline ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu) {
  return npl_pebble_mutex_init(mu);
}

static inline ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
  return npl_pebble_mutex_pend(mu, timeout);
}

static inline ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu) {
  return npl_pebble_mutex_release(mu);
}

static inline ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
  return npl_pebble_sem_init(sem, tokens);
}

static inline ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
  return npl_pebble_sem_pend(sem, timeout);
}

static inline ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem) {
  return npl_pebble_sem_release(sem);
}

static inline uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem) {
  return uxSemaphoreGetCount(sem->handle);
}

static inline void ble_npl_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                                        ble_npl_event_fn *ev_cb, void *ev_arg) {
  npl_pebble_callout_init(co, evq, ev_cb, ev_arg);
}

static inline ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *co,
                                                    ble_npl_time_t ticks) {
  return npl_pebble_callout_reset(co, ticks);
}

static inline void ble_npl_callout_stop(struct ble_npl_callout *co) { npl_pebble_callout_stop(co); }

static inline bool ble_npl_callout_is_active(struct ble_npl_callout *co) {
  return npl_pebble_callout_is_active(co);
}

static inline ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co) {
  return npl_pebble_callout_get_ticks(co);
}

static inline uint32_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co,
                                                       ble_npl_time_t time) {
  return npl_pebble_callout_remaining_ticks(co, time);
}

static inline void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg) {
  co->ev.arg = arg;
}

static inline uint32_t ble_npl_time_get(void) { return xTaskGetTickCountFromISR(); }

static inline ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
  return npl_pebble_time_ms_to_ticks(ms, out_ticks);
}

static inline ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
  return npl_pebble_time_ticks_to_ms(ticks, out_ms);
}

static inline ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms) { return ms; }

static inline uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks) { return ticks; }

static inline void ble_npl_time_delay(ble_npl_time_t ticks) { vTaskDelay(ticks); }

#if NIMBLE_CFG_CONTROLLER
void ble_npl_hw_set_isr(int irqn, void (*addr)(void));
#endif

static inline uint32_t ble_npl_hw_enter_critical(void) {
  vPortEnterCritical();
  return 0;
}

static inline void ble_npl_hw_exit_critical(uint32_t ctx) { vPortExitCritical(); }

static inline bool ble_npl_hw_is_in_critical(void) {
  return vPortInCritical();
}
#define realloc kernel_realloc

#endif /* _NPL_H_ */

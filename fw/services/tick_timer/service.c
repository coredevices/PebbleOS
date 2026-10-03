/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/tick_timer.h"

#include "kernel/events.h"
#include <pbl/drivers/rtc.h>
#include "pbl/kernel/mutex.h"
#include "pbl/services/regular_timer.h"
#include <pbl/logging/logging.h>
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/passert.h"

PBL_LOG_MODULE_DEFINE(service_tick_timer, CONFIG_SERVICE_TICK_TIMER_LOG_LEVEL);

static PBL_MUTEX_DEFINE(s_mutex);
static uint16_t s_num_subscribers;
static PebbleTaskBitset s_second_tasks;
static RegularTimerInfo *s_active_timer;

static void timer_tick_event_publisher(void *data) {
  PebbleEvent e = {
    .type = PEBBLE_TICK_EVENT,
    .clock_tick.tick_time = rtc_get_time(),
  };

  event_put(&e);
}

static RegularTimerInfo s_second_timer_info = {.cb = &timer_tick_event_publisher};
static RegularTimerInfo s_minute_timer_info = {.cb = &timer_tick_event_publisher};

static void prv_update_timer_locked(void) {
  RegularTimerInfo *timer = NULL;
  if (s_num_subscribers > 0) {
    timer = (s_second_tasks != 0) ? &s_second_timer_info : &s_minute_timer_info;
  }
  if (timer == s_active_timer) {
    return;
  }

  if (s_active_timer) {
    regular_timer_remove_callback(s_active_timer);
  }
  if (timer == &s_second_timer_info) {
    PBL_LOG_DBG("ticking every second");
    regular_timer_add_seconds_callback(timer);
  } else if (timer == &s_minute_timer_info) {
    PBL_LOG_DBG("ticking every minute");
    regular_timer_add_minutes_callback(timer);
  } else {
    PBL_LOG_DBG("stopping tick timer");
  }
  s_active_timer = timer;
}

static void prv_set_second_ticks(PebbleTask task, bool second_ticks) {
  const PebbleTaskBitset task_bit = (PebbleTaskBitset)(1u << task);

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (second_ticks) {
    s_second_tasks |= task_bit;
  } else {
    s_second_tasks &= (PebbleTaskBitset)~task_bit;
  }
  prv_update_timer_locked();
  pbl_mutex_unlock(&s_mutex);
}

void tick_timer_add_subscriber(PebbleTask task) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  ++s_num_subscribers;
  prv_update_timer_locked();
  pbl_mutex_unlock(&s_mutex);
}

void tick_timer_remove_subscriber(PebbleTask task) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  PBL_ASSERTN(s_num_subscribers > 0);
  --s_num_subscribers;
  pbl_mutex_unlock(&s_mutex);
  prv_set_second_ticks(task, false);
}

#define REQUEST_SECONDS    (1U << 0)
#define REQUEST_FIRST_TICK (1U << 1)
#define REQUEST_TASK_SHIFT 8U

static void prv_apply_request(void *data) {
  const uintptr_t request = (uintptr_t)data;
  const PebbleTask task = (PebbleTask)(request >> REQUEST_TASK_SHIFT);

  prv_set_second_ticks(task, (request & REQUEST_SECONDS) != 0U);

  if ((request & REQUEST_FIRST_TICK) != 0U) {
    PebbleEvent e = {
      .type = PEBBLE_TICK_EVENT,
      .task_mask = (PebbleTaskBitset) ~(1u << task),
      .clock_tick.tick_time = rtc_get_time(),
    };
    event_put(&e);
  }
}

//! App and worker subscriptions reach KernelMain through the task's queue, so the request
//! follows the same queue to be applied after the subscription changes queued before it.
static void prv_request(uintptr_t flags) {
  const PebbleTask task = pebble_task_get_current();
  const uintptr_t request = ((uintptr_t)task << REQUEST_TASK_SHIFT) | flags;

  if (task == PebbleTask_App || task == PebbleTask_Worker) {
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_apply_request,
        .data = (void *)request,
      },
    };
    if (!event_try_put_from_process(task, &e)) {
      syscall_failed();
    }
  } else {
    prv_apply_request((void *)request);
  }
}

DEFINE_SYSCALL(void, sys_tick_timer_subscribe, TimeUnits units) {
  prv_request(REQUEST_FIRST_TICK | (((units & SECOND_UNIT) != 0) ? REQUEST_SECONDS : 0U));
}

DEFINE_SYSCALL(void, sys_tick_timer_unsubscribe, void) {
  prv_request(0U);
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/evented_timer.h>
#include <pbl/util/misc.h>

#include <kernel/events.h>
#include <kernel/pebble_tasks.h>
#include <process_management/app_manager.h>
#include <syscall/syscall.h>

PBL_LOG_MODULE_DEFINE(service_evented_timer, CONFIG_SERVICE_EVENTED_TIMER_LOG_LEVEL);

typedef struct EventedTimer {
  ListNode list_node;

  EventedTimerID id;

  struct pbl_delayed_work work;
  pbl_tick_t period_ticks;

  EventedTimerCallback callback;
  void *callback_data;

  PebbleTask target_task;

  bool expired; // This gets set when the timer's callback runs
  bool repeating;
  bool released; // Unlinked, and freed once a run in progress is over
} EventedTimer;

//! The list of all the timers that have been created.
static ListNode *s_timer_list_head;

static PBL_MUTEX_DEFINE(s_mutex);

static uint32_t s_last_id;

// ------------------------------------------------------------------------------------
// Find timer by id
static bool prv_id_list_filter(ListNode *node, void *data) {
  EventedTimer *timer = (EventedTimer *)node;
  return timer->id == (uintptr_t)data;
}

static EventedTimer *prv_find_timer(EventedTimerID timer_id) {
  if (timer_id == EVENTED_TIMER_INVALID_ID) {
    return nullptr;
  }

  // Look for this timer in our linked list
  ListNode *node = list_find(s_timer_list_head, prv_id_list_filter, (void *)(intptr_t)timer_id);

  return (EventedTimer *)node;
}

//! Call with s_mutex held.
static EventedTimerID prv_next_id(void) {
  do {
    s_last_id++;
  } while (s_last_id == EVENTED_TIMER_INVALID_ID || prv_find_timer(s_last_id));
  return s_last_id;
}

static void prv_free_work(struct pbl_work *work) {
  pbl_free(container_of(work, EventedTimer, work.work));
}

//! Unlinks and frees a timer, never blocking: prv_work can hold up the queue posting to the very
//! task that gets here, so waiting for it could deadlock. Once canceled, no new run can start. A
//! run already in progress can be waiting for s_mutex and goes on to read the timer, so the free
//! is queued behind it, which also keeps the address from being reused under it. Call with s_mutex
//! held.
static void prv_release_timer(EventedTimer *timer) {
  list_remove(&timer->list_node, &s_timer_list_head, nullptr);
  timer->released = true;
  pbl_delayed_work_cancel(&timer->work);
  if (pbl_work_is_running(&timer->work.work)) {
    timer->work.work.fn = prv_free_work;
    pbl_work_submit(&pbl_sys_workq_high, &timer->work.work);
  } else {
    pbl_free(timer);
  }
}

//! Retrieves details for a given timer handle and copies them out to user supplied memory.
//! This gets executed on the client's task and is called directly from the callback we put onto the
//! client task's event queue.
//! It accesses the privileged contents of the timer from the client's unprivileged task.
//! This call removes the timer from the timer list and releases it before returning unless it is a
//! repeating timer.
PBL_SYSCALL(void, sys_evented_timer_consume, (VAL, uint32_t, timer_id),
            (OUT, EventedTimerCallback *, out_cb), (OUT, void **, out_cb_data)) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  EventedTimer *timer = prv_find_timer(timer_id);

  // It's possible that the client made a call to delete the timer just
  // after the timer executed (from the timer  task) and posted the PEBBLE_CALLBACK_EVENT
  // to the client's event queue. In this case, the timer will no
  // longer be in our timers list by the time the event arrives and gets processed here.
  if (!timer) {
    pbl_mutex_unlock(&s_mutex);
    *out_cb = 0;
    return;
  }

  // Only allow consuming a timer that belongs to the calling task. Without this
  // check an unprivileged app could probe timer IDs to find a system timer and
  // read back its kernel callback pointer + data (KASLR-defeating info leak),
  // and additionally cancel that timer (DoS).
  if (from_user && timer->target_task != pebble_task_get_current()) {
    pbl_mutex_unlock(&s_mutex);
    *out_cb = 0;
    return;
  }

  *out_cb = timer->callback;
  *out_cb_data = timer->callback_data;

  if (!timer->repeating) {
    prv_release_timer(timer);
  }
  pbl_mutex_unlock(&s_mutex);
}

//! Wrapper for the user supplied callback. We installed this callback by posting a
//! PEBBLE_CALLBACK_EVENT to the client's event queue. This gets executed on the target task.
static void prv_evented_timer_event_callback(void *data) {
  // Note this may be running on the app task, so we have to jump through hoops to read kernel
  // memory.

  uint32_t timer_id = (uint32_t)(uintptr_t)data;

  EventedTimerCallback timer_cb;
  void *timer_cb_data;

  // Get the user supplied callback pointer and data, remove the timer from our list, and delete it.
  sys_evented_timer_consume(timer_id, &timer_cb, &timer_cb_data);

  if (!timer_cb) {
    // We've already cancelled this timer, just abort.
    return;
  }

  timer_cb(timer_cb_data);
}

//! Called on the queue's thread. From here we need to generate a callback on the client's task.
//! The timer stays allocated until this returns: see prv_release_timer.
static void prv_work(struct pbl_work *work) {
  PBL_ASSERT_TASK(PebbleTask_SysWorkqHigh);
  EventedTimer *timer = container_of(work, EventedTimer, work.work);

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  // A cancel or reschedule may have got s_mutex first, after this run was dequeued.
  if (timer->released || pbl_delayed_work_is_pending(&timer->work)) {
    pbl_mutex_unlock(&s_mutex);
    return;
  }

  timer->expired = !timer->repeating;
  const EventedTimerID id = timer->id;
  const PebbleTask target_task = timer->target_task;
  const bool repeating = timer->repeating;

  pbl_mutex_unlock(&s_mutex);

  PebbleEvent e = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = prv_evented_timer_event_callback,
      .data = (void *)id,
    }
  };

  switch (target_task) {
    case PebbleTask_KernelMain:
      event_put(&e);
      break;
    case PebbleTask_App:
    case PebbleTask_Worker:
      process_manager_send_event_to_process(target_task, &e);
      break;
    default:
      pbl_panic("Invalid task %s", pebble_task_get_name(pebble_task_get_current()));
  }

  if (!repeating) {
    return;
  }

  // After posting, which can block, so that periods missed meanwhile are skipped.
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (!timer->released && !pbl_delayed_work_is_pending(&timer->work)) {
    pbl_delayed_work_schedule_next(&timer->work, timer->period_ticks);
  }
  pbl_mutex_unlock(&s_mutex);
}

// ========================================================================================================
// External API

void evented_timer_init(void) {
}

void evented_timer_clear_process_timers(PebbleTask task) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  ListNode *iter = s_timer_list_head;
  while (iter) {
    EventedTimer *timer = (EventedTimer *)iter;
    ListNode *next = list_get_next(iter);

    if (timer->target_task == task) {
      prv_release_timer(timer);
    }

    iter = next;
  }

  pbl_mutex_unlock(&s_mutex);
}

EventedTimerID evented_timer_register_or_reschedule(EventedTimerID timer_id, uint32_t timeout_ms,
                                                    EventedTimerCallback callback, void *data) {
  if (timer_id != EVENTED_TIMER_INVALID_ID && evented_timer_reschedule(timer_id, timeout_ms)) {
    return timer_id;
  }
  return evented_timer_register(timeout_ms, false, callback, data);
}

static size_t prv_num_timers_for_task(PebbleTask task) {
  size_t count = 0;
  for (ListNode *node = s_timer_list_head; node != nullptr; node = node->next) {
    if (((EventedTimer *)node)->target_task == task) {
      count++;
    }
  }
  return count;
}

EventedTimerID evented_timer_register(uint32_t timeout_ms, bool repeating,
                                      EventedTimerCallback callback, void *data) {
  PebbleTask current_task = pebble_task_get_current();
  PBL_ASSERT(current_task == PebbleTask_KernelMain || current_task == PebbleTask_App ||
                 current_task == PebbleTask_Worker,
             "Invalid task: %s", pebble_task_get_name(current_task));

  // Handle a lazy client. Timers are useful for handling things "not right now, but soon".
  if (timeout_ms == 0) {
    timeout_ms = 1;
  }

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  if (current_task != PebbleTask_KernelMain &&
      prv_num_timers_for_task(current_task) >= CONFIG_SERVICE_EVENTED_TIMER_MAX_PER_PROCESS) {
    pbl_mutex_unlock(&s_mutex);
    PBL_LOG_WRN("Too many timers for %s", pebble_task_get_name(current_task));
    return EVENTED_TIMER_INVALID_ID;
  }

  // A process must not be able to reset the watch by asking for more timers than the heap holds.
  EventedTimer *timer = current_task == PebbleTask_KernelMain ? pbl_malloc_check(sizeof(*timer))
                                                              : pbl_malloc(sizeof(*timer));
  if (timer == nullptr) {
    pbl_mutex_unlock(&s_mutex);
    PBL_LOG_WRN("No memory for a timer for %s", pebble_task_get_name(current_task));
    return EVENTED_TIMER_INVALID_ID;
  }

  const EventedTimerID id = prv_next_id();
  const pbl_tick_t ticks = pbl_ms_to_ticks(timeout_ms);

  *timer = (EventedTimer){
    .list_node = {},
    .id = id,
    .period_ticks = ticks,
    .callback = callback,
    .callback_data = data,
    .target_task = current_task,
    .repeating = repeating,
    .expired = false
  };
  pbl_delayed_work_init(&timer->work, prv_work);

  s_timer_list_head = list_prepend(s_timer_list_head, &timer->list_node);

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &timer->work, PBL_TICKS(ticks));

  pbl_mutex_unlock(&s_mutex);
  return id;
}

bool evented_timer_reschedule(EventedTimerID timer_id, uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    timeout_ms = 1;
  }
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  // This will detect an invalid timer ID, or one that already ran on the client's task and got
  // deleted
  //  already
  EventedTimer *timer = prv_find_timer(timer_id);
  if (!timer) {
    PBL_LOG_DBG("Attempting to reschedule an invalid timer (id=%u)", (unsigned)timer_id);
    pbl_mutex_unlock(&s_mutex);
    return false;
  }

  PBL_ASSERT(timer->target_task == pebble_task_get_current(), "%u vs %u", timer->target_task,
             pebble_task_get_current());

  // This will detect if the timer callback has already executed on the queue's thread.
  // If the timer is still in our timer's list but is expired, it means the event posted from there
  // has not yet arrived at the client's task.
  // A running one-shot is about to expire, so it is too late for it as well. A run that gets
  // dequeued after this check finds the item pending again and does not fire.
  if (timer->expired || (!timer->repeating && pbl_work_is_running(&timer->work.work))) {
    pbl_mutex_unlock(&s_mutex);
    return false;
  }

  timer->period_ticks = pbl_ms_to_ticks(timeout_ms);
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &timer->work, PBL_TICKS(timer->period_ticks));

  pbl_mutex_unlock(&s_mutex);
  return true;
}

void evented_timer_cancel(EventedTimerID timer_id) {
  if (timer_id == EVENTED_TIMER_INVALID_ID) {
    return;
  }

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  // Find this timer and validate it
  EventedTimer *timer = prv_find_timer(timer_id);
  if (!timer) {
    PBL_LOG_DBG("Attempting to cancel an invalid timer (id=%u)", (unsigned)timer_id);
    pbl_mutex_unlock(&s_mutex);
    return;
  }

  prv_release_timer(timer);

  pbl_mutex_unlock(&s_mutex);
}

bool evented_timer_exists(EventedTimerID timer_id) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  bool exists = (prv_find_timer(timer_id) != nullptr);
  pbl_mutex_unlock(&s_mutex);
  return exists;
}

bool evented_timer_is_current_task(EventedTimerID timer_id) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  EventedTimer *timer = prv_find_timer(timer_id);
  bool is_current_task = (timer != nullptr) && (timer->target_task == pebble_task_get_current());
  pbl_mutex_unlock(&s_mutex);
  return is_current_task;
}

void evented_timer_reset(void) {
  s_timer_list_head = 0;
}

void *evented_timer_get_data(EventedTimerID timer_id) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  EventedTimer *timer = prv_find_timer(timer_id);
  void *data = timer ? timer->callback_data : nullptr;
  pbl_mutex_unlock(&s_mutex);
  return data;
}

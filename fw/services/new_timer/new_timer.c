/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/new_timer/new_timer_service.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <kernel/pebble_tasks.h>
#include <kernel/util/task_init.h>

PBL_LOG_MODULE_DEFINE(service_new_timer, CONFIG_SERVICE_NEW_TIMER_LOG_LEVEL);

#define WORK_POOL_SIZE 5

#define ID_BITS       27
#define ID_INDEX_BITS 8
#define ID_INDEX_MASK ((1U << ID_INDEX_BITS) - 1U)
static_assert(CONFIG_SERVICE_NEW_TIMER_POOL_SIZE < ID_INDEX_MASK, "timer index must fit its id");

struct pbl_new_timer__timer {
  struct pbl_delayed_work dwork;
  NewTimerCallback cb;
  void *cb_data;
  pbl_tick_t period_ticks;
  //! Changes on every use of the slot, so a stale id is caught.
  uint32_t id : ID_BITS;
  uint32_t allocated : 1;
  uint32_t scheduled : 1;
  uint32_t repeating : 1;
  uint32_t executing : 1;
  uint32_t defer_delete : 1;
};

static struct pbl_thread s_new_timer_thread;
PBL_THREAD_STACK_DEFINE(s_new_timer_stack, CONFIG_SERVICE_NEW_TIMER_THREAD_STACK_SIZE);
static PBL_WORK_POOL_DEFINE(s_work_pool, WORK_POOL_SIZE);

static struct pbl_new_timer__timer s_timers[CONFIG_SERVICE_NEW_TIMER_POOL_SIZE];
static struct pbl_new_timer__timer *s_free_head;
static void *s_current_cb;

static void prv_slot_free(struct pbl_new_timer__timer *timer) {
  timer->allocated = false;
  timer->dwork.work.next = (struct pbl_work *)s_free_head;
  s_free_head = timer;
}

static struct pbl_new_timer__timer *prv_find(TimerID id) {
  const uint32_t index = (id & ID_INDEX_MASK) - 1U;

  PBL_ASSERTN(index < ARRAY_LENGTH(s_timers));
  struct pbl_new_timer__timer *timer = &s_timers[index];
  PBL_ASSERTN(timer->allocated && timer->id == id);
  return timer;
}

static void prv_arm(struct pbl_new_timer__timer *timer, pbl_tick_t ticks) {
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &timer->dwork, PBL_TICKS(ticks));
  timer->scheduled = true;
}

static pbl_tick_t prv_ms_to_ticks(uint32_t ms) {
  const uint64_t ticks = ((uint64_t)ms * PBL_TICK_HZ) / 1000U;

  return ticks < PBL_TICK_FOREVER ? (pbl_tick_t)ticks : PBL_TICK_FOREVER - 1U;
}

static void prv_fire(struct pbl_work *work) {
  struct pbl_new_timer__timer *timer = container_of(work, struct pbl_new_timer__timer, dwork.work);

  pbl_irq_lock();
  // A stop or restart from an ISR can land between the dequeue and here.
  if (!timer->scheduled || pbl_delayed_work_is_pending(&timer->dwork)) {
    pbl_irq_unlock();
    return;
  }
  NewTimerCallback cb = timer->cb;
  void *cb_data = timer->cb_data;
  timer->scheduled = false;
  timer->executing = true;
  pbl_irq_unlock();

  s_current_cb = (void *)cb;
  cb(cb_data);
  s_current_cb = nullptr;

  pbl_irq_lock();
  timer->executing = false;
  if (timer->defer_delete) {
    prv_slot_free(timer);
  } else if (timer->repeating && !timer->scheduled) {
    pbl_delayed_work_schedule_next(&timer->dwork, timer->period_ticks);
    timer->scheduled = true;
  }
  pbl_irq_unlock();
}

// =======================================================================================
// Client-side Implementation

TimerID new_timer_create(void) {
  pbl_irq_lock();
  struct pbl_new_timer__timer *timer = s_free_head;
  PBL_ASSERTN(timer != nullptr);
  s_free_head = (struct pbl_new_timer__timer *)timer->dwork.work.next;

  const uint32_t index = (uint32_t)(timer - s_timers) + 1U;
  const TimerID generation = (timer->id >> ID_INDEX_BITS) + 1U;
  const TimerID id = ((generation << ID_INDEX_BITS) | index) & ((1U << ID_BITS) - 1U);
  *timer = (struct pbl_new_timer__timer){.id = id, .allocated = true};
  pbl_delayed_work_init(&timer->dwork, prv_fire);
  pbl_irq_unlock();

  return id;
}

bool new_timer_start(TimerID timer_id, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags) {
  const pbl_tick_t ticks = prv_ms_to_ticks(timeout_ms);
  bool started = false;

  pbl_irq_lock();
  struct pbl_new_timer__timer *timer = prv_find(timer_id);
  PBL_ASSERTN(!timer->defer_delete);

  if (!((flags & TIMER_START_FLAG_FAIL_IF_EXECUTING) && timer->executing) &&
      !((flags & TIMER_START_FLAG_FAIL_IF_SCHEDULED) && timer->scheduled)) {
    timer->cb = cb;
    timer->cb_data = cb_data;
    timer->repeating = (flags & TIMER_START_FLAG_REPEATING) != 0U;
    timer->period_ticks = ticks;
    prv_arm(timer, ticks);
    started = true;
  }
  pbl_irq_unlock();

  return started;
}

bool new_timer_scheduled(TimerID timer_id, uint32_t *expire_ms_p) {
  pbl_irq_lock();
  struct pbl_new_timer__timer *timer = prv_find(timer_id);
  PBL_ASSERTN(!timer->defer_delete);

  const bool scheduled = timer->scheduled;
  if (scheduled && expire_ms_p != nullptr) {
    *expire_ms_p = pbl_ticks_to_ms(pbl_delayed_work_remaining(&timer->dwork));
  }
  pbl_irq_unlock();

  return scheduled;
}

bool new_timer_stop(TimerID timer_id) {
  pbl_irq_lock();
  struct pbl_new_timer__timer *timer = prv_find(timer_id);
  PBL_ASSERTN(!timer->defer_delete);

  pbl_delayed_work_cancel(&timer->dwork);
  timer->scheduled = false;
  timer->repeating = false;
  const bool executing = timer->executing;
  pbl_irq_unlock();

  return !executing;
}

void new_timer_delete(TimerID timer_id) {
  pbl_irq_lock();
  struct pbl_new_timer__timer *timer = prv_find(timer_id);

  if (!timer->defer_delete) {
    pbl_delayed_work_cancel(&timer->dwork);
    timer->scheduled = false;
    timer->repeating = false;
    if (timer->executing) {
      timer->defer_delete = true;
    } else {
      prv_slot_free(timer);
    }
  }
  pbl_irq_unlock();
}

// ========================================================================================
// Service Implementation

static void *prv_wdt_expired(int channel_id, void *user_data) {
  return new_timer_debug_get_current_callback();
}

static void prv_workq_start(struct pbl_workq *q) {
  task_init();
  PBL_ASSERTN(pbl_task_wdt_add(nullptr, CONFIG_TASK_WDT_TIMEOUT_MS, prv_wdt_expired, nullptr) >= 0);
}

static void prv_workq_wait(struct pbl_workq *q, bool waiting) {
  pbl_task_wdt_set_waiting(waiting);
}

static const struct pbl_workq_ops s_workq_ops = {
  .start = prv_workq_start,
  .wait = prv_workq_wait,
};

void *new_timer_debug_get_current_callback(void) {
  return s_current_cb ? s_current_cb : pbl_workq_current_fn(&pbl_sys_workq_high);
}

void new_timer_service_init(void) {
  PBL_LOG_DBG("NT: Initializing");

  for (size_t i = ARRAY_LENGTH(s_timers); i > 0; i--) {
    prv_slot_free(&s_timers[i - 1U]);
  }

  struct pbl_thread_attr attr = {
    .name = "NewTimer",
    .entry = pbl_workq_thread,
    .arg = &pbl_sys_workq_high,
    .prio = CONFIG_SERVICE_NEW_TIMER_THREAD_PRIO,
    .privileged = true,
    .stack = s_new_timer_stack,
    .stack_size = sizeof(s_new_timer_stack),
  };

  pbl_workq_set_ops(&pbl_sys_workq_high, &s_workq_ops);
  PBL_ASSERTN(pbl_thread_create(&s_new_timer_thread, &attr) == 0);
  pebble_task_register(PebbleTask_NewTimers, &s_new_timer_thread);
}

void new_timer_add_work_callback_from_isr(NewTimerWorkCallback cb, void *data) {
  (void)pbl_work_call(&pbl_sys_workq_high, &s_work_pool, cb, data, PBL_NO_WAIT);
}

bool new_timer_add_work_callback(NewTimerWorkCallback cb, void *data) {
  return pbl_work_call(&pbl_sys_workq_high, &s_work_pool, cb, data, PBL_TICKS(50)) == 0;
}

#if UNITTEST
void new_timer_reset_for_test(void) {
  memset(&s_new_timer_thread, 0, sizeof(s_new_timer_thread));
  static struct pbl_work_call s_slots[WORK_POOL_SIZE];

  pbl_workq_init(&pbl_sys_workq_high);
  pbl_work_pool_init(&s_work_pool, s_slots, WORK_POOL_SIZE);
  s_free_head = nullptr;
  for (size_t i = 0; i < ARRAY_LENGTH(s_timers); i++) {
    s_timers[i] = (struct pbl_new_timer__timer){0};
  }
  s_current_cb = nullptr;
}
#endif

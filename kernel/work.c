/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <string.h>

#include <pbl/kernel/work.h>
#include <pbl/mcu/fpu.h>
#include <pbl/util/misc.h>

PBL_WORKQ_DEFINE(pbl_sys_workq);
PBL_WORKQ_DEFINE(pbl_sys_workq_high);

static struct pbl_thread s_sys_thread;
static struct pbl_thread s_sys_high_thread;
PBL_THREAD_STACK_DEFINE(s_sys_stack, CONFIG_KERNEL_SYS_WORKQ_STACK_SIZE);
PBL_THREAD_STACK_DEFINE(s_sys_high_stack, CONFIG_KERNEL_SYS_WORKQ_HIGH_STACK_SIZE);

struct pbl_work__barrier {
  struct pbl_work work;
  struct pbl_sem done;
};

static void prv_barrier_fn(struct pbl_work *work) {
  struct pbl_work__barrier *barrier = (struct pbl_work__barrier *)work;

  pbl_sem_give(&barrier->done);
}

static void prv_barrier_init(struct pbl_work__barrier *barrier, struct pbl_workq *q) {
  pbl_work_init(&barrier->work, prv_barrier_fn);
  barrier->work.q = q;
  barrier->work.pending = true;
  pbl_sem_init(&barrier->done, 0, 1);
}

static void prv_barrier_wait(struct pbl_work__barrier *barrier) {
  pbl_sem_take(&barrier->done, PBL_FOREVER);
  pbl_sem_deinit(&barrier->done);
}

//! Links @p work after @p prev, or first when @p prev is NULL. Call with the interrupt lock held.
static void prv_insert(struct pbl_workq *q, struct pbl_work *prev, struct pbl_work *work) {
  struct pbl_work **link = prev ? &prev->next : &q->head;

  work->next = *link;
  *link = work;
  if (q->tail == prev) {
    q->tail = work;
  }
  pbl_sem_give(&q->wake);
}

//! Call with the interrupt lock held.
static void prv_boost_apply(struct pbl_workq *q) {
  q->unboosted_prio = pbl_thread_prio_get(q->thread);
  if (q->boost_prio > q->unboosted_prio) {
    pbl_thread_prio_set(q->thread, q->boost_prio);
  }
}

//! Call with the interrupt lock held.
static void prv_boost(struct pbl_workq *q, bool boost) {
  if (boost) {
    if (q->boosts++ == 0U && q->thread != nullptr) {
      prv_boost_apply(q);
    }
  } else {
    PBL_ASSERTN(q->boosts > 0U);
    if (--q->boosts == 0U && q->thread != nullptr && q->boost_prio > q->unboosted_prio) {
      pbl_thread_prio_set(q->thread, q->unboosted_prio);
    }
  }
}

//! Call with the interrupt lock held.
static bool prv_remove(struct pbl_work *work) {
  if (!work->pending) {
    return false;
  }

  struct pbl_workq *q = work->q;
  struct pbl_work *prev = nullptr;
  struct pbl_work **link = &q->head;
  while (*link != work) {
    prev = *link;
    link = &prev->next;
  }
  *link = work->next;
  if (q->tail == work) {
    q->tail = prev;
  }
  work->pending = false;
  if (work->boosted) {
    work->boosted = false;
    prv_boost(q, false);
  }
  return true;
}

//! Returns whether a wait is needed. Call with the interrupt lock held.
static bool prv_running_elsewhere(const struct pbl_work *work) {
  struct pbl_workq *q = work->q;

  return q != nullptr && q->current == work && q->thread != pbl_thread_current();
}

void pbl_workq_init(struct pbl_workq *q) {
  *q = (struct pbl_workq)PBL_WORKQ_INITIALIZER;
  pbl_sem_init(&q->wake, 0, 1);
}

void pbl_workq_set_ops(struct pbl_workq *q, const struct pbl_workq_ops *ops) {
  q->ops = ops;
  // An idle thread is already past its hook, so have it go around again.
  pbl_sem_give(&q->wake);
}

struct pbl_thread *pbl_workq_get_thread(const struct pbl_workq *q) {
  return q->thread;
}

bool pbl_workq_is_current(const struct pbl_workq *q) {
  return !pbl_in_isr() && pbl_thread_current() == q->thread;
}

void pbl_workq_thread(void *arg) {
  struct pbl_workq *q = arg;

  pbl_irq_lock();
  if (q->thread == nullptr) {
    q->thread = pbl_thread_current();
    // Boosted work submitted before the queue had a thread.
    if (q->boosts > 0U) {
      prv_boost_apply(q);
    }
  }
  pbl_irq_unlock();

  for (;;) {
    pbl_irq_lock();
    struct pbl_work *work = q->head;
    pbl_work_fn_t fn = nullptr;
    bool boosted = false;
    if (work) {
      boosted = work->boosted;
      work->boosted = false;
      q->head = work->next;
      if (!q->head) {
        q->tail = nullptr;
      }
      work->pending = false;
      fn = work->fn;
      q->current = work;
      q->current_fn = (void *)fn;
    }
    pbl_irq_unlock();

    if (!work) {
      if (q->ops && q->ops->wait) {
        q->ops->wait(q, true);
      }
      pbl_sem_take(&q->wake, PBL_FOREVER);
      if (q->ops && q->ops->wait) {
        q->ops->wait(q, false);
      }
      continue;
    }

    // The function may have freed the item: only compare its address from here on.
    fn(work);
    // Or the next items pay for stacking the FPU registers on every context switch.
    mcu_fpu_cleanup();

    pbl_irq_lock();
    q->current = nullptr;
    q->current_fn = nullptr;
    if (boosted) {
      prv_boost(q, false);
    }
    pbl_irq_unlock();

    if (q->ops && q->ops->done) {
      q->ops->done(q);
    }
  }
}

static void prv_sys_workq_start(struct pbl_workq *q, struct pbl_thread *thread, const char *name,
                                pbl_prio_t prio, void *stack, size_t stack_size) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = pbl_workq_thread,
    .arg = q,
    .prio = prio,
    .privileged = true,
    .stack = stack,
    .stack_size = stack_size,
  };

  // Set here too, as callers can ask for it before the thread first runs.
  q->thread = thread;
  PBL_ASSERTN(pbl_thread_create(thread, &attr) == 0);
}

void sys_workq_init(void) {
  pbl_workq_set_boost_prio(&pbl_sys_workq, CONFIG_KERNEL_SYS_WORKQ_BOOST_PRIO);
  prv_sys_workq_start(&pbl_sys_workq, &s_sys_thread, "SysWorkq", CONFIG_KERNEL_SYS_WORKQ_PRIO,
                      s_sys_stack, sizeof(s_sys_stack));
  prv_sys_workq_start(&pbl_sys_workq_high, &s_sys_high_thread, "SysWorkqHigh",
                      CONFIG_KERNEL_SYS_WORKQ_HIGH_PRIO, s_sys_high_stack,
                      sizeof(s_sys_high_stack));
}

#if UNITTEST
void sys_workq_reset_for_test(void) {
  memset(&s_sys_thread, 0, sizeof(s_sys_thread));
  memset(&s_sys_high_thread, 0, sizeof(s_sys_high_thread));
  pbl_workq_init(&pbl_sys_workq);
  pbl_workq_init(&pbl_sys_workq_high);
}
#endif

void pbl_workq_flush(struct pbl_workq *q) {
  struct pbl_work__barrier barrier;

  PBL_ASSERTN(!pbl_in_isr() && pbl_thread_current() != q->thread);
  prv_barrier_init(&barrier, q);

  pbl_irq_lock();
  prv_insert(q, q->tail, &barrier.work);
  pbl_irq_unlock();

  prv_barrier_wait(&barrier);
}

void *pbl_workq_current_fn(const struct pbl_workq *q) {
  return q->current_fn;
}

void pbl_work_init(struct pbl_work *work, pbl_work_fn_t fn) {
  *work = (struct pbl_work)PBL_WORK_INITIALIZER(fn);
}

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  bool queued = false;

  pbl_irq_lock();
  if (!work->pending) {
    work->q = q;
    work->pending = true;
    prv_insert(q, q->tail, work);
    queued = true;
  }
  pbl_irq_unlock();
  return queued;
}

void pbl_workq_set_boost_prio(struct pbl_workq *q, pbl_prio_t prio) {
  q->boost_prio = prio;
}

bool pbl_work_submit_boosted(struct pbl_workq *q, struct pbl_work *work) {
  bool queued = false;

  pbl_irq_lock();
  if (!work->pending) {
    work->q = q;
    work->pending = true;
    prv_insert(q, q->tail, work);
    queued = true;
  }
  if (!work->boosted) {
    work->boosted = true;
    prv_boost(work->q, true);
  }
  pbl_irq_unlock();
  return queued;
}

bool pbl_work_cancel(struct pbl_work *work) {
  pbl_irq_lock();
  bool was_pending = prv_remove(work);
  pbl_irq_unlock();
  return was_pending;
}

//! Waits for the running function, if on another thread, to return.
static void prv_wait_running(const struct pbl_work *work) {
  struct pbl_work__barrier barrier;

  pbl_irq_lock();
  if (!prv_running_elsewhere(work)) {
    pbl_irq_unlock();
    return;
  }
  // First in the queue, so it runs right after the function returns.
  prv_barrier_init(&barrier, work->q);
  prv_insert(work->q, nullptr, &barrier.work);
  pbl_irq_unlock();

  prv_barrier_wait(&barrier);
}

bool pbl_work_cancel_sync(struct pbl_work *work) {
  PBL_ASSERTN(!pbl_in_isr());

  bool was_pending = pbl_work_cancel(work);
  prv_wait_running(work);
  // The function may have submitted the item again before returning.
  return pbl_work_cancel(work) || was_pending;
}

bool pbl_work_flush(struct pbl_work *work) {
  struct pbl_work__barrier barrier;

  PBL_ASSERTN(!pbl_in_isr());

  pbl_irq_lock();
  struct pbl_workq *q = work->q;
  if (q == nullptr || (!work->pending && q->current != work)) {
    pbl_irq_unlock();
    return false;
  }
  PBL_ASSERTN(pbl_thread_current() != q->thread);
  prv_barrier_init(&barrier, q);
  prv_insert(q, work->pending ? work : nullptr, &barrier.work);
  pbl_irq_unlock();

  prv_barrier_wait(&barrier);
  return true;
}

bool pbl_work_is_pending(const struct pbl_work *work) {
  return work->pending;
}

bool pbl_work_is_running(const struct pbl_work *work) {
  pbl_irq_lock();
  bool running = work->q != nullptr && work->q->current == work;
  pbl_irq_unlock();
  return running;
}

// ---- delayed work -----------------------------------------------------------

void pbl_delayed_work_expired(struct pbl_timer *timer) {
  struct pbl_delayed_work *dwork = container_of(timer, struct pbl_delayed_work, timer);

  pbl_irq_lock();
  // An ISR preempting the tick may have canceled or restarted the delay since the timer expired.
  if (dwork->work.armed && !pbl_timer_is_active(timer)) {
    dwork->work.armed = false;
    pbl_work_submit(dwork->work.q, &dwork->work);
  }
  pbl_irq_unlock();
}

void pbl_delayed_work_init(struct pbl_delayed_work *dwork, pbl_work_fn_t fn) {
  *dwork = (struct pbl_delayed_work)PBL_DELAYED_WORK_INITIALIZER(fn);
}

static void prv_arm(struct pbl_workq *q, struct pbl_delayed_work *dwork, pbl_timeout_t delay) {
  if (pbl_timeout_is_no_wait(delay)) {
    pbl_work_submit(q, &dwork->work);
  } else {
    dwork->work.q = q;
    dwork->work.armed = true;
    pbl_timer_start(&dwork->timer, delay);
  }
}

bool pbl_delayed_work_is_pending(const struct pbl_delayed_work *dwork) {
  pbl_irq_lock();
  bool pending = dwork->work.pending || dwork->work.armed;
  pbl_irq_unlock();
  return pending;
}

bool pbl_delayed_work_schedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                               pbl_timeout_t delay) {
  pbl_irq_lock();
  bool schedule = !pbl_delayed_work_is_pending(dwork);
  if (schedule) {
    prv_arm(q, dwork, delay);
  }
  pbl_irq_unlock();
  return schedule;
}

bool pbl_delayed_work_cancel(struct pbl_delayed_work *dwork) {
  pbl_irq_lock();
  bool was_pending = pbl_delayed_work_is_pending(dwork);
  dwork->work.armed = false;
  pbl_timer_stop(&dwork->timer);
  prv_remove(&dwork->work);
  pbl_irq_unlock();
  return was_pending;
}

bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                 pbl_timeout_t delay) {
  pbl_irq_lock();
  bool was_pending = pbl_delayed_work_cancel(dwork);
  prv_arm(q, dwork, delay);
  pbl_irq_unlock();
  return was_pending;
}

void pbl_delayed_work_schedule_next(struct pbl_delayed_work *dwork, pbl_tick_t period) {
  pbl_irq_lock();
  const uint64_t next = dwork->timer.expires + period;
  const uint64_t now = sched_uptime_ticks();
  const uint64_t delay = next > now ? next - now : 1U;

  pbl_delayed_work_reschedule(dwork->work.q, dwork,
                              PBL_TICKS(delay < PBL_TICK_FOREVER ? delay : PBL_TICK_FOREVER - 1U));
  pbl_irq_unlock();
}

bool pbl_delayed_work_cancel_sync(struct pbl_delayed_work *dwork) {
  PBL_ASSERTN(!pbl_in_isr());

  bool was_pending = pbl_delayed_work_cancel(dwork);
  prv_wait_running(&dwork->work);
  // The function may have scheduled the item again before returning.
  return pbl_delayed_work_cancel(dwork) || was_pending;
}

pbl_tick_t pbl_delayed_work_remaining(const struct pbl_delayed_work *dwork) {
  return pbl_timer_remaining(&dwork->timer);
}

// ---- calls ------------------------------------------------------------------

void pbl_work_pool_init(struct pbl_work_pool *pool, struct pbl_work_call *slots, size_t num_slots) {
  *pool = (struct pbl_work_pool){
    .slots = slots,
    .num_slots = num_slots,
  };
  pbl_sem_init(&pool->free, num_slots, num_slots);
}

size_t pbl_work_pool_num_free(const struct pbl_work_pool *pool) {
  return pbl_sem_count(&pool->free);
}

static void prv_call_fn(struct pbl_work *work) {
  struct pbl_work_call *call = container_of(work, struct pbl_work_call, work);
  struct pbl_work_pool *pool = call->pool;
  pbl_work_call_fn_t fn = call->fn;
  void *data = call->data;

  pbl_irq_lock();
  work->q->current_fn = (void *)fn;
  // Not an item callers can wait on, and the slot can be reused from here on.
  work->q->current = nullptr;
  call->work.next = (struct pbl_work *)pool->free_head;
  pool->free_head = call;
  pbl_irq_unlock();
  pbl_sem_give(&pool->free);

  fn(data);
}

static struct pbl_work_call *prv_call_alloc(struct pbl_work_pool *pool) {
  struct pbl_work_call *call;

  pbl_irq_lock();
  if (pool->num_used < pool->num_slots) {
    call = &pool->slots[pool->num_used++];
  } else {
    call = pool->free_head;
    pool->free_head = (struct pbl_work_call *)call->work.next;
  }
  pbl_irq_unlock();
  return call;
}

int pbl_work_call(struct pbl_workq *q, struct pbl_work_pool *pool, pbl_work_call_fn_t fn,
                  void *data, pbl_timeout_t timeout) {
  if (!pbl_in_isr() && pbl_thread_current() == q->thread) {
    timeout = PBL_NO_WAIT;
  }

  int rc = pbl_sem_take(&pool->free, timeout);
  if (rc != 0) {
    return rc;
  }

  struct pbl_work_call *call = prv_call_alloc(pool);

  call->pool = pool;
  call->fn = fn;
  call->data = data;
  pbl_work_init(&call->work, prv_call_fn);
  pbl_work_submit(q, &call->work);
  return 0;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <string.h>

#include <pbl/kernel/compiler.h>

#define NUM_PRIO CONFIG_KERNEL_NUM_PRIORITIES
static_assert(NUM_PRIO <= 32, "the ready bitmap is 32 bits wide");

struct pbl_thread *pbl_all_threads;
struct pbl_thread *sched_next;

static struct pbl_thread *s_ready_head[NUM_PRIO];
static struct pbl_thread *s_ready_tail[NUM_PRIO];
static uint32_t s_ready_mask;
static uint64_t s_ticks;
static uint32_t s_sched_lock;
static bool s_switch_deferred;
static bool s_started;
static uint32_t s_next_number;

static struct pbl_thread s_idle_thread;
PBL_THREAD_STACK_DEFINE(s_idle_stack, CONFIG_KERNEL_IDLE_STACK_SIZE);

// ---- ready lists ------------------------------------------------------------

static void prv_ready_push(struct pbl_thread *t) {
  pbl_prio_t p = t->prio;
  t->backend.next = nullptr;
  if (s_ready_tail[p]) {
    s_ready_tail[p]->backend.next = t;
  } else {
    s_ready_head[p] = t;
    s_ready_mask |= 1u << p;
  }
  s_ready_tail[p] = t;
  t->backend.state = PBL_THREAD_READY;
  if (sched_next == nullptr || p > sched_next->prio) {
    sched_next = t;
  }
}

static void prv_ready_remove(struct pbl_thread *t) {
  pbl_prio_t p = t->prio;
  struct pbl_thread **link = &s_ready_head[p];
  struct pbl_thread *prev = nullptr;
  while (*link && *link != t) {
    prev = *link;
    link = &(*link)->backend.next;
  }
  if (*link == nullptr) {
    return;
  }
  *link = t->backend.next;
  if (s_ready_tail[p] == t) {
    s_ready_tail[p] = prev;
  }
  if (s_ready_head[p] == nullptr) {
    s_ready_mask &= ~(1u << p);
  }
  t->backend.next = nullptr;
  if (sched_next == t) {
    sched_next = s_ready_mask ? s_ready_head[31 - PBL_CLZ(s_ready_mask)] : nullptr;
  }
}

static void prv_rotate(pbl_prio_t p) {
  struct pbl_thread *t = s_ready_head[p];
  if (t && t->backend.next) {
    prv_ready_remove(t);
    prv_ready_push(t);
  }
}

void sched_request_switch(void) {
  if (s_sched_lock) {
    s_switch_deferred = true;
    return;
  }
  if (s_started && sched_next != pbl_cur) {
    arch_switch_request();
  }
}

// ---- wait queues ------------------------------------------------------------

void waitq_insert(struct pbl_waitq *wq, struct pbl_thread *t) {
  struct pbl_thread **link = &wq->head;
  while (*link && (*link)->prio >= t->prio) {
    link = &(*link)->backend.next;
  }
  t->backend.next = *link;
  *link = t;
}

void waitq_remove(struct pbl_waitq *wq, struct pbl_thread *t) {
  struct pbl_thread **link = &wq->head;
  while (*link && *link != t) {
    link = &(*link)->backend.next;
  }
  if (*link) {
    *link = t->backend.next;
    t->backend.next = nullptr;
  }
}

struct pbl_thread *waitq_pop(struct pbl_waitq *wq) {
  struct pbl_thread *t = wq->head;
  if (t) {
    wq->head = t->backend.next;
    t->backend.next = nullptr;
  }
  return t;
}

// ---- blocking and waking ----------------------------------------------------

int sched_block(struct pbl_waitq *wq, pbl_timeout_t timeout, unsigned int key) {
  struct pbl_thread *t = pbl_cur;
  PBL_ASSERTN(!arch_in_isr());
  PBL_ASSERTN(s_sched_lock == 0);

  prv_ready_remove(t);
  t->backend.state = PBL_THREAD_BLOCKED;
  t->backend.waitq = wq;
  t->backend.wake_rc = 0;
  if (wq) {
    waitq_insert(wq, t);
  }
  if (!pbl_timeout_is_forever(timeout)) {
    timeout_add(&t->backend.timeout, timeout.ticks);
  }
  arch_switch_request();

  // Drop the lock so the switch can happen; we come back here once woken.
  arch_irq_unlock(key);
  (void)arch_irq_lock();

  return t->backend.wake_rc;
}

void sched_wake(struct pbl_thread *t, int rc) {
  PBL_ASSERTN(t->backend.state == PBL_THREAD_BLOCKED);
  timeout_remove(&t->backend.timeout);
  t->backend.waitq = nullptr;
  t->backend.wake_rc = rc;
  prv_ready_push(t);
  if (t->prio > pbl_cur->prio) {
    sched_request_switch();
  }
}

// ---- priorities ------------------------------------------------------------

void sched_prio_set(struct pbl_thread *t, pbl_prio_t base, pbl_prio_t effective) {
  t->backend.base_prio = base;
  if (effective == t->prio) {
    return;
  }
  if (t->backend.state == PBL_THREAD_READY) {
    prv_ready_remove(t);
    t->prio = effective;
    prv_ready_push(t);
  } else if (t->backend.state == PBL_THREAD_BLOCKED && t->backend.waitq) {
    waitq_remove(t->backend.waitq, t);
    t->prio = effective;
    waitq_insert(t->backend.waitq, t);
  } else {
    t->prio = effective;
  }
  sched_request_switch();
}

// ---- thread lifecycle -------------------------------------------------------

void sched_thread_start(struct pbl_thread *t) {
  t->backend.timeout.thread = t;
  t->backend.number = ++s_next_number;
  t->backend.base_prio = t->prio;
  t->backend.all_next = pbl_all_threads;
  pbl_all_threads = t;
  prv_ready_push(t);
  sched_request_switch();
}

static void prv_detach(struct pbl_thread *t) {
  switch (t->backend.state) {
    case PBL_THREAD_READY:
      prv_ready_remove(t);
      break;
    case PBL_THREAD_BLOCKED:
      timeout_remove(&t->backend.timeout);
      if (t->backend.waitq) {
        waitq_remove(t->backend.waitq, t);
        t->backend.waitq = nullptr;
      }
      break;
    default:
      break;
  }
}

void sched_thread_remove(struct pbl_thread *t) {
  prv_detach(t);
  t->backend.state = PBL_THREAD_DEAD;
  struct pbl_thread **link = &pbl_all_threads;
  while (*link && *link != t) {
    link = &(*link)->backend.all_next;
  }
  if (*link) {
    *link = t->backend.all_next;
  }
  sched_request_switch();
}

void sched_thread_suspend(struct pbl_thread *t) {
  if (t->backend.state == PBL_THREAD_SUSPENDED || t->backend.state == PBL_THREAD_DEAD) {
    return;
  }
  // A thread suspended while blocked sees the call fail once resumed.
  bool was_blocked = t->backend.state == PBL_THREAD_BLOCKED;
  prv_detach(t);
  t->backend.state = PBL_THREAD_SUSPENDED;
  t->backend.wake_rc = was_blocked ? KWAKE_INTERRUPTED : 0;
  sched_request_switch();
}

void sched_thread_resume(struct pbl_thread *t) {
  if (t->backend.state != PBL_THREAD_SUSPENDED) {
    return;
  }
  prv_ready_push(t);
  sched_request_switch();
}

void sched_yield_current(void) {
  prv_rotate(pbl_cur->prio);
  sched_request_switch();
}

// ---- arch entry points ------------------------------------------------------

void sched_announce(uint64_t ticks) {
  s_ticks += ticks;
  // Whoever runs when a tick ends gets it.
  pbl_cur->backend.run_time += (uint32_t)ticks;
}

void sched_timed_out(struct pbl_thread *t) {
  if (t->backend.waitq) {
    waitq_remove(t->backend.waitq, t);
    t->backend.waitq = nullptr;
  }
  t->backend.wake_rc = -EAGAIN;
  prv_ready_push(t);
  sched_request_switch();
}

void sched_slice(void) {
  unsigned int key = arch_irq_lock();
  // Round-robin among threads sharing the running priority.
  prv_rotate(pbl_cur->prio);
  sched_request_switch();
  arch_irq_unlock(key);
}

bool sched_idle_confirm(void) {
  return sched_next == &s_idle_thread && !s_switch_deferred;
}

uint64_t sched_uptime_ticks(void) {
  return s_ticks;
}

struct pbl_thread *sched_idle_thread(void) {
  return &s_idle_thread;
}

static void prv_idle_entry(void *arg) {
  (void)arg;
  for (;;) {
    unsigned int key = arch_irq_lock();
    // Give other threads at the idle priority their turn first.
    if (s_ready_head[PBL_PRIO_IDLE]->backend.next) {
      sched_yield_current();
    }
    pbl_tick_t idle_ticks = timeout_ticks_until_next();
    arch_irq_unlock(key);
    arch_idle(idle_ticks);
  }
}

// ---- public scheduler API ---------------------------------------------------

void sched_start_prepare(void) {
  struct pbl_thread_attr attr = {
    .name = "IDLE",
    .entry = prv_idle_entry,
    .prio = PBL_PRIO_IDLE,
    .privileged = true,
    .stack = s_idle_stack,
    .stack_size = sizeof(s_idle_stack),
  };
  int rc = pbl_thread_create(&s_idle_thread, &attr);
  PBL_ASSERTN(rc == 0);

  (void)arch_irq_lock();
  s_started = true;
  arch_cur_set(sched_next);
}

void pbl_kernel_start(void) {
  sched_start_prepare();
  timekeeping_start();
  arch_start();
}

bool pbl_kernel_is_started(void) {
  return s_started;
}

bool pbl_kernel_is_running(void) {
  return s_started && s_sched_lock == 0;
}

void pbl_sched_lock(void) {
  unsigned int key = arch_irq_lock();
  s_sched_lock++;
  arch_irq_unlock(key);
}

void pbl_sched_unlock(void) {
  unsigned int key = arch_irq_lock();
  PBL_ASSERTN(s_sched_lock > 0);
  if (--s_sched_lock == 0 && s_switch_deferred) {
    s_switch_deferred = false;
    sched_request_switch();
  }
  arch_irq_unlock(key);
}

bool pbl_sched_is_locked(void) {
  return s_sched_lock > 0;
}

uint64_t pbl_uptime_ticks(void) {
  unsigned int key = arch_irq_lock();
  uint64_t ticks = s_ticks;
  arch_irq_unlock(key);
  return ticks;
}

void pbl_thread_yield(void) {
  unsigned int key = arch_irq_lock();
  sched_yield_current();
  arch_irq_unlock(key);
}

void pbl_thread_sleep(pbl_timeout_t timeout) {
  if (pbl_timeout_is_no_wait(timeout)) {
    pbl_thread_yield();
    return;
  }
  unsigned int key = arch_irq_lock();
  sched_block(nullptr, timeout, key);
  arch_irq_unlock(key);
}

void sched_reset_for_test(void) {
  memset(&s_idle_thread, 0, sizeof(s_idle_thread));
  arch_cur_set(nullptr);
  pbl_all_threads = nullptr;
  sched_next = nullptr;
  for (int i = 0; i < NUM_PRIO; i++) {
    s_ready_head[i] = s_ready_tail[i] = nullptr;
  }
  s_ready_mask = 0;
  s_ticks = 0;
  s_sched_lock = 0;
  s_switch_deferred = false;
  s_started = false;
  s_next_number = 0;
  timeout_reset_for_test();
  wallclock_reset_for_test();
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/timer.h>

static struct pbl_timer *s_timers;

static inline bool prv_before(pbl_tick_t a, pbl_tick_t b) {
  return (int32_t)(a - b) < 0;
}

static void prv_remove(struct pbl_timer *timer) {
  struct pbl_timer **link = &s_timers;
  while (*link && *link != timer) {
    link = &(*link)->next;
  }
  if (*link) {
    *link = timer->next;
  }
  timer->active = false;
}

void pbl_timer_init(struct pbl_timer *timer, pbl_timer_fn_t fn) {
  *timer = (struct pbl_timer)PBL_TIMER_INITIALIZER(fn);
}

void pbl_timer_start(struct pbl_timer *timer, pbl_timeout_t delay) {
  PBL_ASSERTN(!pbl_timeout_is_forever(delay));

  pbl_irq_lock();
  if (timer->active) {
    prv_remove(timer);
  }
  timer->expires = sched_ticks() + (delay.ticks > 0 ? delay.ticks : 1);
  struct pbl_timer **link = &s_timers;
  while (*link && !prv_before(timer->expires, (*link)->expires)) {
    link = &(*link)->next;
  }
  timer->next = *link;
  *link = timer;
  timer->active = true;
  pbl_irq_unlock();
}

void pbl_timer_stop(struct pbl_timer *timer) {
  pbl_irq_lock();
  if (timer->active) {
    prv_remove(timer);
  }
  pbl_irq_unlock();
}

bool pbl_timer_is_active(const struct pbl_timer *timer) {
  return timer->active;
}

void timer_expire(void) {
  for (;;) {
    pbl_irq_lock();
    struct pbl_timer *timer = s_timers;
    if (timer == nullptr || prv_before(sched_ticks(), timer->expires)) {
      pbl_irq_unlock();
      return;
    }
    s_timers = timer->next;
    timer->active = false;
    pbl_irq_unlock();

    timer->fn(timer);
  }
}

pbl_tick_t timer_ticks_until_next(pbl_tick_t now) {
  if (s_timers == nullptr) {
    return PBL_TICK_FOREVER;
  }
  int32_t delta = (int32_t)(s_timers->expires - now);
  return delta > 0 ? (pbl_tick_t)delta : 0;
}

void timer_reset_for_test(void) {
  s_timers = nullptr;
}

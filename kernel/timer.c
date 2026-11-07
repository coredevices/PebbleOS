/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/timer.h>

static struct pbl_timer *s_timers;

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
  timer->expires = sched_uptime_ticks() + (delay.ticks > 0 ? delay.ticks : 1);
  struct pbl_timer **link = &s_timers;
  while (*link && (*link)->expires <= timer->expires) {
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

pbl_tick_t pbl_timer_remaining(const struct pbl_timer *timer) {
  pbl_tick_t remaining = 0;

  pbl_irq_lock();
  uint64_t now = sched_uptime_ticks();
  if (timer->active && timer->expires > now) {
    remaining = (pbl_tick_t)(timer->expires - now);
  }
  pbl_irq_unlock();
  return remaining;
}

void timer_expire(void) {
  for (;;) {
    pbl_irq_lock();
    struct pbl_timer *timer = s_timers;
    if (timer == nullptr || sched_uptime_ticks() < timer->expires) {
      pbl_irq_unlock();
      return;
    }
    s_timers = timer->next;
    timer->active = false;
    pbl_irq_unlock();

    timer->fn(timer);
  }
}

pbl_tick_t timer_ticks_until_next(uint64_t now) {
  if (s_timers == nullptr) {
    return PBL_TICK_FOREVER;
  }
  if (s_timers->expires <= now) {
    return 0;
  }
  uint64_t delta = s_timers->expires - now;
  return delta < PBL_TICK_FOREVER ? (pbl_tick_t)delta : PBL_TICK_FOREVER - 1;
}

void timer_reset_for_test(void) {
  s_timers = nullptr;
}

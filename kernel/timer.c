/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/timer.h>
#include <pbl/util/misc.h>

// Sorted by expiry, first in first out for the same tick.
static struct pbl_timeout *s_timeouts;

void timeout_add(struct pbl_timeout *to, pbl_tick_t ticks) {
  struct pbl_timeout **link = &s_timeouts;

  to->expires = sched_uptime_ticks() + ticks;
  while (*link && (*link)->expires <= to->expires) {
    link = &(*link)->next;
  }
  to->next = *link;
  *link = to;
  to->active = true;
}

void timeout_remove(struct pbl_timeout *to) {
  struct pbl_timeout **link = &s_timeouts;

  if (!to->active) {
    return;
  }
  while (*link && *link != to) {
    link = &(*link)->next;
  }
  if (*link) {
    *link = to->next;
  }
  to->active = false;
}

void timeout_expire(void) {
  for (;;) {
    unsigned int key = arch_irq_lock();
    struct pbl_timeout *to = s_timeouts;
    if (to == nullptr || sched_uptime_ticks() < to->expires) {
      arch_irq_unlock(key);
      return;
    }
    s_timeouts = to->next;
    to->active = false;
    if (to->thread != nullptr) {
      sched_timed_out(to->thread);
      arch_irq_unlock(key);
    } else {
      struct pbl_timer *timer = container_of(to, struct pbl_timer, timeout);
      arch_irq_unlock(key);
      timer->fn(timer);
    }
  }
}

pbl_tick_t timeout_ticks_until_next(void) {
  if (s_timeouts == nullptr) {
    return PBL_TICK_FOREVER;
  }
  uint64_t now = sched_uptime_ticks();
  if (s_timeouts->expires <= now) {
    return 0;
  }
  uint64_t delta = s_timeouts->expires - now;
  return delta < PBL_TICK_FOREVER ? (pbl_tick_t)delta : PBL_TICK_FOREVER - 1;
}

void timeout_reset_for_test(void) {
  s_timeouts = nullptr;
}

void pbl_timer_init(struct pbl_timer *timer, pbl_timer_fn_t fn) {
  *timer = (struct pbl_timer)PBL_TIMER_INITIALIZER(fn);
}

void pbl_timer_start(struct pbl_timer *timer, pbl_timeout_t delay) {
  PBL_ASSERTN(!pbl_timeout_is_forever(delay));

  unsigned int key = arch_irq_lock();
  timeout_remove(&timer->timeout);
  timeout_add(&timer->timeout, delay.ticks > 0 ? delay.ticks : 1);
  arch_irq_unlock(key);
}

void pbl_timer_stop(struct pbl_timer *timer) {
  unsigned int key = arch_irq_lock();
  timeout_remove(&timer->timeout);
  arch_irq_unlock(key);
}

bool pbl_timer_is_active(const struct pbl_timer *timer) {
  return timer->timeout.active;
}

pbl_tick_t pbl_timer_remaining(const struct pbl_timer *timer) {
  pbl_tick_t remaining = 0;

  unsigned int key = arch_irq_lock();
  uint64_t now = sched_uptime_ticks();
  if (timer->timeout.active && timer->timeout.expires > now) {
    remaining = (pbl_tick_t)(timer->timeout.expires - now);
  }
  arch_irq_unlock(key);
  return remaining;
}

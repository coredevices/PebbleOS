/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/util/misc.h>

static struct pbl_wallclock_work *s_running;

//! Arms the timer for the due boundary. Call with the interrupt lock held.
static void prv_arm(struct pbl_wallclock_work *w) {
  const uint64_t at = timekeeping_realtime_to_ticks(w->due_ms);
  const uint64_t now = sched_uptime_ticks();
  const uint64_t delay = at > now ? at - now : 0U;

  pbl_timer_start(&w->timer, PBL_TICKS(delay < PBL_TICK_FOREVER ? delay : PBL_TICK_FOREVER - 1U));
}

//! The boundary @p every units after the one at or before @p ms.
static uint64_t prv_boundary(const struct pbl_wallclock_work *w, uint64_t ms, uint32_t every) {
  return (ms / w->unit_ms + every) * w->unit_ms;
}

static void prv_expired(struct pbl_timer *timer) {
  struct pbl_wallclock_work *w = container_of(timer, struct pbl_wallclock_work, timer);

  pbl_irq_lock();
  // Stopped, restarted or moved by a clock change since the timer expired.
  if (w->running && !pbl_timer_is_active(timer)) {
    pbl_work_submit(w->q, &w->work);
    // Late enough to have missed boundaries, it skips them.
    const uint64_t now = pbl_realtime_ms();
    do {
      w->due_ms += (uint64_t)w->every * w->unit_ms;
    } while (w->due_ms <= now);
    prv_arm(w);
  }
  pbl_irq_unlock();
}

void pbl_wallclock_work_init(struct pbl_wallclock_work *w, pbl_work_fn_t fn) {
  *w = (struct pbl_wallclock_work){};
  pbl_work_init(&w->work, fn);
  pbl_timer_init(&w->timer, prv_expired);
}

static void prv_unlink(struct pbl_wallclock_work *w) {
  for (struct pbl_wallclock_work **link = &s_running; *link != nullptr; link = &(*link)->next) {
    if (*link == w) {
      *link = w->next;
      return;
    }
  }
}

void pbl_wallclock_work_start(struct pbl_workq *q, struct pbl_wallclock_work *w, uint32_t unit_s,
                              uint32_t every) {
  PBL_ASSERTN(unit_s > 0U && every > 0U);

  pbl_irq_lock();
  if (!w->running) {
    w->next = s_running;
    s_running = w;
  }
  w->q = q;
  w->unit_ms = unit_s * 1000U;
  w->every = every;
  w->running = true;
  w->due_ms = prv_boundary(w, pbl_realtime_ms(), every);
  prv_arm(w);
  pbl_irq_unlock();
}

bool pbl_wallclock_work_stop(struct pbl_wallclock_work *w) {
  pbl_irq_lock();
  const bool was_running = w->running;
  if (was_running) {
    prv_unlink(w);
    w->running = false;
  }
  pbl_timer_stop(&w->timer);
  pbl_work_cancel(&w->work);
  pbl_irq_unlock();
  return was_running;
}

bool pbl_wallclock_work_is_running(const struct pbl_wallclock_work *w) {
  return w->running;
}

void wallclock_clock_set(uint64_t old_ms, uint64_t new_ms) {
  for (struct pbl_wallclock_work *w = s_running; w != nullptr; w = w->next) {
    const uint64_t old_unit = old_ms / w->unit_ms;
    const uint64_t due_unit = w->due_ms / w->unit_ms;
    // None when the item is due and its timer has yet to run.
    uint64_t left = due_unit > old_unit ? due_unit - old_unit : 0U;

    // A change that lands in another unit counts as one boundary, as a clock that ticked would.
    if (left > 0U && new_ms / w->unit_ms != old_unit) {
      left--;
    }
    if (left == 0U) {
      pbl_work_submit(w->q, &w->work);
      left = w->every;
    }
    w->due_ms = prv_boundary(w, new_ms, (uint32_t)left);
    prv_arm(w);
  }
}

#if UNITTEST
void wallclock_reset_for_test(void) {
  s_running = nullptr;
}
#endif

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/util/size.h>

// Delayed work that only runs when a test says so. A pending item's delay is kept in its timer.

#define FAKE_WORK_MAX_PENDING 32

static struct pbl_delayed_work *s_fake_work_pending[FAKE_WORK_MAX_PENDING];
static const struct pbl_delayed_work *s_fake_work_running;
static int s_fake_work_num_arm_calls;
static int s_fake_work_num_cancel_calls;

static void prv_fake_work_remove(struct pbl_delayed_work *dwork) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fake_work_pending); i++) {
    if (s_fake_work_pending[i] == dwork) {
      s_fake_work_pending[i] = nullptr;
    }
  }
  dwork->work.armed = false;
}

static void prv_fake_work_arm(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                              pbl_tick_t ticks) {
  prv_fake_work_remove(dwork);
  dwork->work.q = q;
  dwork->work.armed = true;
  dwork->timer.expires = ticks;
  s_fake_work_num_arm_calls++;
  for (size_t i = 0; i < ARRAY_LENGTH(s_fake_work_pending); i++) {
    if (s_fake_work_pending[i] == nullptr) {
      s_fake_work_pending[i] = dwork;
      return;
    }
  }
  PBL_ASSERTN(false);
}

void pbl_delayed_work_expired(struct pbl_timer *timer) {
}

void pbl_delayed_work_init(struct pbl_delayed_work *dwork, pbl_work_fn_t fn) {
  *dwork = (struct pbl_delayed_work)PBL_DELAYED_WORK_INITIALIZER(fn);
}

bool pbl_delayed_work_is_pending(const struct pbl_delayed_work *dwork) {
  return dwork->work.armed;
}

bool pbl_delayed_work_schedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                               pbl_timeout_t delay) {
  if (dwork->work.armed) {
    return false;
  }
  prv_fake_work_arm(q, dwork, delay.ticks);
  return true;
}

bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                 pbl_timeout_t delay) {
  const bool was_pending = dwork->work.armed;

  prv_fake_work_arm(q, dwork, delay.ticks);
  return was_pending;
}

void pbl_delayed_work_schedule_next(struct pbl_delayed_work *dwork, pbl_tick_t period) {
  prv_fake_work_arm(dwork->work.q, dwork, period);
}

bool pbl_delayed_work_cancel(struct pbl_delayed_work *dwork) {
  const bool was_pending = dwork->work.armed;

  s_fake_work_num_cancel_calls++;
  prv_fake_work_remove(dwork);
  return was_pending;
}

bool pbl_delayed_work_cancel_sync(struct pbl_delayed_work *dwork) {
  return pbl_delayed_work_cancel(dwork);
}

pbl_tick_t pbl_delayed_work_remaining(const struct pbl_delayed_work *dwork) {
  return dwork->work.armed ? (pbl_tick_t)dwork->timer.expires : 0U;
}

bool pbl_work_is_running(const struct pbl_work *work) {
  return s_fake_work_running != nullptr && &s_fake_work_running->work == work;
}

// ---- for tests --------------------------------------------------------------

//! Whether the item is waiting for its delay.
static inline bool fake_work_is_scheduled(const struct pbl_delayed_work *dwork) {
  return dwork->work.armed;
}

//! The delay the item was last scheduled with.
static inline uint32_t fake_work_timeout_ms(const struct pbl_delayed_work *dwork) {
  // Rounded up, which gets back the exact value pbl_ms_to_ticks() rounded down.
  return (uint32_t)((dwork->timer.expires * 1000U + PBL_TICK_HZ - 1U) / PBL_TICK_HZ);
}

//! The pending item with the shortest delay, NULL if none is pending.
static inline struct pbl_delayed_work *fake_work_get_next(void) {
  struct pbl_delayed_work *next = nullptr;

  for (size_t i = 0; i < ARRAY_LENGTH(s_fake_work_pending); i++) {
    struct pbl_delayed_work *dwork = s_fake_work_pending[i];
    if (dwork != nullptr && (next == nullptr || dwork->timer.expires < next->timer.expires)) {
      next = dwork;
    }
  }
  return next;
}

//! Runs a pending item's function, as if its delay had passed.
static inline void fake_work_fire(struct pbl_delayed_work *dwork) {
  PBL_ASSERTN(dwork->work.armed);
  prv_fake_work_remove(dwork);
  s_fake_work_running = dwork;
  dwork->work.fn(&dwork->work);
  s_fake_work_running = nullptr;
}

//! Makes an item read as running, for what a function may race with.
static inline void fake_work_set_running(const struct pbl_delayed_work *dwork) {
  s_fake_work_running = dwork;
}

//! Forgets every pending item. Call from the test's cleanup.
static inline void fake_work_cleanup(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fake_work_pending); i++) {
    if (s_fake_work_pending[i] != nullptr) {
      prv_fake_work_remove(s_fake_work_pending[i]);
    }
  }
  s_fake_work_running = nullptr;
  s_fake_work_num_arm_calls = 0;
  s_fake_work_num_cancel_calls = 0;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/timer.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>

#include <clar.h>
#include <kernel.h>

#define CYCLES_PER_TICK 1024U

static uint32_t s_counter;
static uint64_t s_ticks;
static uint32_t s_last_delta;
static pbl_tick_t s_until_next_timer;
static bool s_timer_due_on_wake;

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return s_counter;
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  s_last_delta = delta;
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
}

static int prv_cs_init(const struct pbl_device *dev);
static int prv_ce_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {.read = prv_read};
static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_cs);
static const struct pbl_clocksource s_cs =
    PBL_CLOCKSOURCE_INIT(s_cs, "cs", prv_cs_init, &s_cs_ops, 32);
PBL_DEVICE_STATE_DEFINE(s_ce);
static const struct pbl_clockevent s_ce =
    PBL_CLOCKEVENT_INIT(s_ce, "ce", prv_ce_init, &s_ce_ops, 1, UINT32_MAX);

const struct pbl_clocksource *const pbl_system_clocksource = &s_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_ce;

static int prv_cs_init(const struct pbl_device *dev) {
  pbl_clocksource_set_hz(&s_cs, PBL_TICK_HZ * CYCLES_PER_TICK);
  return 0;
}

static int prv_ce_init(const struct pbl_device *dev) {
  pbl_clockevent_set_hz(&s_ce, PBL_TICK_HZ * CYCLES_PER_TICK);
  return 0;
}

// Sleeping takes a whole tick, past the expiry of the timer.
static void prv_enter(void) {
  s_counter += CYCLES_PER_TICK;
  s_until_next_timer = s_timer_due_on_wake ? 0U : PBL_TICK_FOREVER;
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  {.name = "test", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter},
};
const size_t pbl_soc_num_idle_states = 1;

int pbl_soc_idle_state_limit(void) {
  return 0;
}

uint64_t sched_uptime_ticks(void) {
  return s_ticks;
}

void sched_announce(uint64_t ticks) {
  s_ticks += ticks;
}

pbl_tick_t timer_ticks_until_next(uint64_t now) {
  return s_until_next_timer;
}

void timer_expire(void) {
}

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

void test_timekeeping__initialize(void) {
  s_counter = 0U;
  s_ticks = 0U;
  timekeeping_start();
}

void test_timekeeping__waking_up_with_a_timer_due_raises_the_event_at_once(void) {
  s_timer_due_on_wake = true;
  timekeeping_idle(1U);
  cl_assert_equal_i(s_ticks, 1);
  cl_assert_equal_i(s_last_delta, 1);
}

void test_timekeeping__waking_up_with_no_timer_due_raises_the_next_tick(void) {
  s_timer_due_on_wake = false;
  timekeeping_idle(1U);
  cl_assert_equal_i(s_ticks, 1);
  cl_assert_equal_i(s_last_delta, CYCLES_PER_TICK);
}

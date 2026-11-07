/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/drivers/timer.h>
#include <pbl/kernel/idle.h>
#include <pbl/util/math.h>

#define FRAC_BITS 32U

static const struct pbl_clocksource *s_cs;
static const struct pbl_clockevent *s_ce;
static uint64_t s_cs_mhz;
static uint32_t s_ce_hz;
static bool s_running;

// Ticks per clock source cycle, in 32.32 fixed point.
static uint64_t s_mult;

// Last raw counter value, and the clock source time in ticks plus a 32-bit fraction.
static uint32_t s_cs_last;
static uint64_t s_cs_ticks;
static uint32_t s_cs_frac;

static uint64_t s_residency_us[PBL_IDLE_STATES_MAX];

// Wall clock time minus uptime, in microseconds.
static int64_t s_realtime_offset_us;

// Brings the clock source time up to date.
static void prv_update(void) {
  uint32_t raw = s_cs->ops->read(s_cs);
  uint64_t acc = s_cs_frac + ((raw - s_cs_last) & s_cs->mask) * s_mult;

  s_cs_last = raw;
  s_cs_ticks += acc >> FRAC_BITS;
  s_cs_frac = (uint32_t)acc;
}

// Clock source time in microseconds.
static uint64_t prv_now_us(void) {
  prv_update();

  return (s_cs_ticks * 1000000U + (((uint64_t)s_cs_frac * 1000000U) >> FRAC_BITS)) / PBL_TICK_HZ;
}

// Uptime in microseconds, also before the clock source runs.
static uint64_t prv_uptime_us(void) {
  if (!s_running) {
    return (sched_uptime_ticks() * 1000000U) / PBL_TICK_HZ;
  }

  return prv_now_us();
}

static void prv_announce(void) {
  uint64_t announced = sched_uptime_ticks();

  prv_update();
  if (s_cs_ticks > announced) {
    sched_announce(s_cs_ticks - announced);
  }
}

// Raise the next clock event at the start of tick @p ticks.
static void prv_program(uint64_t ticks) {
  // Read the counter again before it can wrap, and stay within the device limits.
  uint64_t max_ticks = MIN(((uint64_t)(s_cs->mask / 2U) * PBL_TICK_HZ * 1000U) / s_cs_mhz,
                           ((uint64_t)s_ce->max_delta * PBL_TICK_HZ) / s_ce_hz);
  uint64_t whole;
  uint32_t frac;
  uint64_t delta;

  prv_update();
  if (ticks <= s_cs_ticks) {
    whole = 0U;
    frac = 0U;
  } else if (ticks - s_cs_ticks > max_ticks) {
    whole = max_ticks;
    frac = 0U;
  } else {
    // Ticks to go, minus the fraction of the current one already elapsed.
    whole = ticks - s_cs_ticks;
    frac = 0U;
    if (s_cs_frac != 0U) {
      whole--;
      frac = (uint32_t)(-s_cs_frac);
    }
  }

  delta = whole * s_ce_hz + (((uint64_t)frac * s_ce_hz + UINT32_MAX) >> FRAC_BITS);
  delta = (delta + PBL_TICK_HZ - 1U) / PBL_TICK_HZ;
  delta = CLIP(delta, s_ce->min_delta, s_ce->max_delta);

  s_ce->ops->set_next(s_ce, (uint32_t)delta);
}

void pbl_clocksource_set_mhz(const struct pbl_clocksource *cs, uint64_t mhz) {
  PBL_ASSERTN(mhz != 0U);

  if (cs != pbl_system_clocksource) {
    return;
  }

  pbl_irq_lock();
  if (s_running) {
    prv_update();
  }
  s_cs_mhz = mhz;
  s_mult = (((uint64_t)PBL_TICK_HZ * 1000U) << FRAC_BITS) / mhz;
  pbl_irq_unlock();
}

void pbl_clocksource_set_hz(const struct pbl_clocksource *cs, uint32_t hz) {
  pbl_clocksource_set_mhz(cs, (uint64_t)hz * 1000U);
}

void pbl_clockevent_set_hz(const struct pbl_clockevent *ce, uint32_t hz) {
  PBL_ASSERTN(hz != 0U);

  if (ce != pbl_system_clockevent) {
    return;
  }

  pbl_irq_lock();
  s_ce_hz = hz;
  pbl_irq_unlock();
}

void pbl_clockevent_handler(const struct pbl_clockevent *ce) {
  pbl_irq_lock();
  if (s_running && ce == s_ce) {
    prv_announce();
    prv_program(sched_uptime_ticks() + 1U);
  }
  pbl_irq_unlock();

  timer_expire();
}

void timekeeping_start(void) {
  int ret;

  s_cs = pbl_system_clocksource;
  s_ce = pbl_system_clockevent;

  ret = pbl_device_init(&s_cs->dev);
  PBL_ASSERTN(ret == 0);
  ret = pbl_device_init(&s_ce->dev);
  PBL_ASSERTN(ret == 0);
  PBL_ASSERTN(s_cs_mhz != 0U && s_ce_hz != 0U);
  PBL_ASSERTN(pbl_soc_num_idle_states <= PBL_IDLE_STATES_MAX);

  pbl_irq_lock();
  s_cs_last = s_cs->ops->read(s_cs);
  s_cs_ticks = sched_uptime_ticks();
  s_cs_frac = 0U;
  s_running = true;
  prv_program(s_cs_ticks + 1U);
  pbl_irq_unlock();
}

void timekeeping_announce(void) {
  pbl_irq_lock();
  prv_announce();
  pbl_irq_unlock();
}

void timekeeping_idle(pbl_tick_t ticks) {
  const struct pbl_idle_state *state = nullptr;
  int limit;
  int i;

  if (!s_running || ticks == 0U) {
    return;
  }

  limit = pbl_soc_idle_state_limit();
  for (i = MIN(limit, (int)pbl_soc_num_idle_states - 1); i >= 0; i--) {
    if (ticks >= pbl_soc_idle_states[i].min_ticks) {
      state = &pbl_soc_idle_states[i];
      break;
    }
  }
  if (state == nullptr) {
    return;
  }

  pbl_irq_lock();
  uint64_t wake = UINT64_MAX / 2U;
  if (ticks != PBL_TICK_FOREVER) {
    wake = sched_uptime_ticks() + ticks - MIN(state->exit_ticks, ticks - 1U);
  }
  prv_program(wake);
  uint64_t start_us = prv_now_us();
  pbl_irq_unlock();

  // The lock would keep the kernel's interrupts from waking the CPU up.
  state->enter();

  pbl_irq_lock();
  s_residency_us[i] += prv_now_us() - start_us;
  prv_announce();
  // Programming drops the tick that woke the CPU up, and only the ISR runs timers.
  uint64_t now = sched_uptime_ticks();
  prv_program(timer_ticks_until_next(now) == 0U ? now : now + 1U);
  pbl_irq_unlock();
}

uint64_t pbl_realtime_ms(void) {
  int64_t us;

  pbl_irq_lock();
  us = (int64_t)prv_uptime_us() + s_realtime_offset_us;
  pbl_irq_unlock();

  return (us > 0) ? (uint64_t)us / 1000U : 0U;
}

time_t pbl_realtime(void) {
  return (time_t)(pbl_realtime_ms() / 1000U);
}

void pbl_realtime_set(time_t t) {
  pbl_irq_lock();
  s_realtime_offset_us = (int64_t)t * 1000000 - (int64_t)prv_uptime_us();
  pbl_irq_unlock();
}

uint64_t pbl_idle_residency_us(size_t state) {
  uint64_t us;

  if (state >= PBL_IDLE_STATES_MAX) {
    return 0U;
  }

  pbl_irq_lock();
  us = s_residency_us[state];
  pbl_irq_unlock();

  return us;
}

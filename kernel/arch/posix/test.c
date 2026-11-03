/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "posix.h"

#include <stdio.h>
#include <stdlib.h>

#include <pbl/drivers/timer.h>
#include <pbl/kernel/idle.h>

#include <kernel.h>
#include <kernel_test.h>
#include <pthread.h>
#include <unistd.h>

// Unit test harness: time only moves when the test ticks it, or when every
// thread is idle.

// pthreads of threads that died without being joined yet.
static pthread_t s_zombies[64];
static size_t s_num_zombies;

void posix_add_zombie(pthread_t tid) {
  if (s_num_zombies < sizeof(s_zombies) / sizeof(s_zombies[0])) {
    s_zombies[s_num_zombies++] = tid;
  }
}

static pthread_cond_t s_stop_cond = PTHREAD_COND_INITIALIZER;
static bool s_stopped;

// A counter at the tick rate, advanced by the harness; events are raised by pbl_test_tick().
static uint32_t s_fake_counter;

static uint32_t prv_fake_read(const struct pbl_clocksource *cs) {
  return s_fake_counter;
}

static void prv_fake_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
}

static void prv_fake_shutdown(const struct pbl_clockevent *ce) {
}

static int prv_fake_cs_init(const struct pbl_device *dev);
static int prv_fake_ce_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_fake_cs_ops = {.read = prv_fake_read};
static const struct pbl_clockevent_ops s_fake_ce_ops = {
  .set_next = prv_fake_set_next,
  .shutdown = prv_fake_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_fake_cs);
static const struct pbl_clocksource s_fake_cs =
    PBL_CLOCKSOURCE_INIT(s_fake_cs, "fake", prv_fake_cs_init, &s_fake_cs_ops, 32);
PBL_DEVICE_STATE_DEFINE(s_fake_ce);
static const struct pbl_clockevent s_fake_ce =
    PBL_CLOCKEVENT_INIT(s_fake_ce, "fake", prv_fake_ce_init, &s_fake_ce_ops, 1, UINT32_MAX);

const struct pbl_clocksource *const pbl_system_clocksource = &s_fake_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_fake_ce;

static int prv_fake_cs_init(const struct pbl_device *dev) {
  pbl_clocksource_set_hz(&s_fake_cs, PBL_TICK_HZ);
  return 0;
}

static int prv_fake_ce_init(const struct pbl_device *dev) {
  pbl_clockevent_set_hz(&s_fake_ce, PBL_TICK_HZ);
  return 0;
}

const struct pbl_idle_state pbl_soc_idle_states[] = {};
const size_t pbl_soc_num_idle_states = 0;

int pbl_soc_idle_state_limit(void) {
  return -1;
}

// Idle means nothing can run until the next timeout: jump the clock there.
void arch_idle(pbl_tick_t max_ticks) {
  if (s_stopped) {
    pthread_mutex_lock(&posix_cpu);
    pthread_cond_signal(&s_stop_cond);
    pthread_mutex_unlock(&posix_cpu);
    for (;;) {
      pause();
    }
  }
  if (max_ticks == PBL_TICK_FOREVER) {
    fprintf(stderr, "kernel test: every thread is blocked forever\n");
    abort();
  }
  // The last tick is an interrupt, so that timers expire in ISR context.
  if (max_ticks > 1) {
    s_fake_counter += max_ticks - 1;
    timekeeping_announce();
  }
  pbl_test_tick(1);
}

static void prv_run_until_stopped(void) {
  pthread_mutex_lock(&posix_cpu);
  posix_run(pbl_cur);
  while (!s_stopped) {
    pthread_cond_wait(&s_stop_cond, &posix_cpu);
  }
  pthread_mutex_unlock(&posix_cpu);
  for (struct pbl_thread *t = pbl_all_threads; t != nullptr; t = t->backend.all_next) {
    struct posix_thread *pt = t->backend.arch.pt;
    if (pt != nullptr && pt->created) {
      pthread_cancel(pt->tid);
      pthread_join(pt->tid, nullptr);
    }
  }
}

void arch_start(void) {
  irq_reset();
  prv_run_until_stopped();
  // Only the test harness gets here; it re-enters through pbl_test_kernel_run().
  pthread_exit(nullptr);
}

void pbl_test_kernel_run(void) {
  s_stopped = false;
  posix_switch_pending = false;
  posix_in_isr = false;
  sched_start_prepare();
  timekeeping_start();
  irq_reset();
  prv_run_until_stopped();
  for (size_t i = 0; i < s_num_zombies; i++) {
    pthread_cancel(s_zombies[i]);
    pthread_join(s_zombies[i], nullptr);
  }
  s_num_zombies = 0;
  sched_reset_for_test();
}

void pbl_test_kernel_stop(void) {
  s_stopped = true;
  pthread_cond_signal(&s_stop_cond);
  // Park this thread; the harness tears it down.
  struct posix_thread *pt = pbl_cur->backend.arch.pt;
  pt->run = false;
  posix_park(pt);
}

void pbl_test_isr_enter(void) {
  posix_in_isr = true;
}

void pbl_test_isr_exit(void) {
  posix_in_isr = false;
  if (posix_switch_pending) {
    posix_switch();
  }
}

void pbl_test_tick(uint32_t ticks) {
  for (uint32_t i = 0; i < ticks; i++) {
    s_fake_counter++;
    pbl_test_isr_enter();
    pbl_clockevent_handler(&s_fake_ce);
    pbl_test_isr_exit();
  }
}

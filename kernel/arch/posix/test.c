/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pbl/kernel/idle.h"

#include "kernel.h"
#include "kernel_test.h"
#include "posix.h"

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
  pbl_irq_lock();
  sched_idle_slept(max_ticks);
  pbl_irq_unlock();
}

static void prv_run_until_stopped(void) {
  pthread_mutex_lock(&posix_cpu);
  posix_run(pbl_cur);
  while (!s_stopped) {
    pthread_cond_wait(&s_stop_cond, &posix_cpu);
  }
  pthread_mutex_unlock(&posix_cpu);
  for (struct pbl_thread *t = pbl_all_threads; t != NULL; t = t->backend.all_next) {
    struct posix_thread *pt = t->backend.arch.pt;
    if (pt != NULL && pt->created) {
      pthread_cancel(pt->tid);
      pthread_join(pt->tid, NULL);
    }
  }
}

void arch_start(void) {
  irq_reset();
  prv_run_until_stopped();
  // Only the test harness gets here; it re-enters through pbl_test_kernel_run().
  pthread_exit(NULL);
}

void pbl_test_kernel_run(void) {
  s_stopped = false;
  posix_switch_pending = false;
  posix_in_isr = false;
  sched_start_prepare();
  irq_reset();
  prv_run_until_stopped();
  for (size_t i = 0; i < s_num_zombies; i++) {
    pthread_cancel(s_zombies[i]);
    pthread_join(s_zombies[i], NULL);
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
    pbl_test_isr_enter();
    pbl_kernel_tick_isr();
    pbl_test_isr_exit();
  }
}

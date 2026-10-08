/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
// Ahead of pthread.h, which needs struct timespec from the host's time.h,
// not the firmware's.
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include <pbl/kernel/idle.h>
#include <pbl/kernel/init.h>

#include <kernel.h>
#include <pbl_arch_posix.h>
#include "posix.h"

// Native application: interrupts come from host threads, which take the CPU
// whenever no kernel thread holds it, as an interrupt between instructions.

static pthread_cond_t s_irq_cond = PTHREAD_COND_INITIALIZER;
static uint64_t s_irq_count;

void posix_add_zombie(pthread_t tid) {
  pthread_detach(tid);
}

void pbl_posix_boot(int (*entry)(void)) {
  pthread_mutex_lock(&posix_cpu);
  pbl_soc_early_init();
  entry();
  KERNEL_ASSERT(false);
  for (;;) {
  }
}

void pbl_posix_irq_run(void (*isr)(void *), void *arg) {
  pthread_mutex_lock(&posix_cpu);
  posix_in_isr = true;
  isr(arg);
  posix_in_isr = false;
  s_irq_count++;
  pthread_cond_broadcast(&s_irq_cond);
  pthread_mutex_unlock(&posix_cpu);
}

void pbl_posix_cpu_wait(uint64_t timeout_us) {
  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t ns = (uint64_t)now.tv_usec * 1000 + timeout_us * 1000;
  struct timespec deadline = {
    .tv_sec = now.tv_sec + (time_t)(ns / 1000000000),
    .tv_nsec = (long)(ns % 1000000000),
  };

  uint64_t count = s_irq_count;
  while (s_irq_count == count) {
    if (pthread_cond_timedwait(&s_irq_cond, &posix_cpu, &deadline) == ETIMEDOUT) {
      break;
    }
  }
}

void arch_idle(pbl_tick_t max_ticks) {
  pbl_soc_idle(max_ticks);
}

void arch_start(void) {
  irq_reset();
  posix_run(pbl_cur);
  pthread_mutex_unlock(&posix_cpu);
  // The booting host thread is done: the kernel threads take it from here.
  for (;;) {
    pause();
  }
}

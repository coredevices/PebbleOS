/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define _GNU_SOURCE

#include "posix.h"

#include <string.h>

#include <kernel.h>
#include <pthread.h>

// The thread state is host memory, not firmware heap.
#undef malloc
#undef free
#undef calloc
#include <stdlib.h>

// One pthread per kernel thread; the one holding posix_cpu is the one the
// kernel considers running. Switches hand posix_cpu over explicitly, so the
// scheduling is as deterministic as on the target.

struct pbl_thread *pbl_cur;

pthread_mutex_t posix_cpu = PTHREAD_MUTEX_INITIALIZER;
bool posix_switch_pending;
bool posix_in_isr;

// The cleanup handler pushed in prv_thread_main() gives posix_cpu back.
static void prv_exit(struct posix_thread *pt) {
  pthread_cond_destroy(&pt->wake);
  free(pt);
  pthread_exit(nullptr);
}

void posix_park(struct posix_thread *pt) {
  while (!pt->run && !pt->aborted) {
    pthread_cond_wait(&pt->wake, &posix_cpu);
  }
  if (pt->aborted) {
    prv_exit(pt);
  }
}

// Gives posix_cpu back whenever a kernel pthread ends, cancelled or exited.
// Nothing else may unlock it on the way out, or the next owner loses it.
static void prv_release_cpu(void *arg) {
  (void)arg;
  pthread_mutex_unlock(&posix_cpu);
}

static void *prv_thread_main(void *arg) {
  struct posix_thread *pt = arg;
#ifdef __APPLE__
  pthread_setname_np(pt->name);
#else
  pthread_setname_np(pthread_self(), pt->name);
#endif
  pthread_mutex_lock(&posix_cpu);
  pthread_cleanup_push(prv_release_cpu, nullptr);
  posix_park(pt);
  pt->entry(pt->arg);
  pbl_thread_abort(nullptr);
  pthread_cleanup_pop(1);
  return nullptr;
}

void arch_init(void) {
}

void arch_thread_init(struct pbl_thread *t, void (*entry)(void *), void *arg) {
  struct posix_thread *pt = calloc(1, sizeof(*pt));
  PBL_ASSERTN(pt != nullptr);
  pthread_cond_init(&pt->wake, nullptr);
  pt->entry = entry;
  pt->arg = arg;
  strncpy(pt->name, t->name, sizeof(pt->name) - 1);
  t->backend.arch.pt = pt;
}

void arch_thread_regions_set(struct pbl_thread *t, const MpuRegion *const *regions) {
  (void)t;
  (void)regions;
}

void posix_run(struct pbl_thread *t) {
  struct posix_thread *pt = t->backend.arch.pt;
  pt->run = true;
  if (!pt->created) {
    pt->created = true;
    pthread_create(&pt->tid, nullptr, prv_thread_main, pt);
  } else {
    pthread_cond_signal(&pt->wake);
  }
}

void posix_switch(void) {
  posix_switch_pending = false;
  struct pbl_thread *prev = pbl_cur;
  struct pbl_thread *next = sched_switch_in();
  if (next == prev) {
    return;
  }
  struct posix_thread *pt = prev->backend.arch.pt;
  posix_run(next);
  if (prev->backend.state == PBL_THREAD_DEAD) {
    prev->backend.arch.pt = nullptr;
    posix_add_zombie(pt->tid);
    prv_exit(pt);
  }
  pt->run = false;
  posix_park(pt);
}

void arch_switch_request(void) {
  posix_switch_pending = true;
}

void arch_thread_exit(void) {
  // pbl_thread_abort() already switched away and ended this pthread.
  pthread_exit(nullptr);
}

void arch_thread_aborted(struct pbl_thread *t) {
  struct posix_thread *pt = t->backend.arch.pt;
  if (!pt->created) {
    pthread_cond_destroy(&pt->wake);
    free(pt);
  } else {
    // It ends itself once it gets the CPU back.
    posix_add_zombie(pt->tid);
    pt->aborted = true;
    pthread_cond_signal(&pt->wake);
  }
  t->backend.arch.pt = nullptr;
}

bool arch_in_isr(void) {
  return posix_in_isr;
}

uint32_t arch_cycles_per_sec(void) {
  return 1000000000U;
}

void arch_irq_disable(void) {
}

void arch_irq_enable(void) {
  if (!posix_in_isr && posix_switch_pending && pbl_kernel_is_started()) {
    posix_switch();
  }
}

void arch_thread_saved_regs(const struct pbl_thread *t, struct pbl_thread_saved_regs *regs) {
  (void)t;
  *regs = (struct pbl_thread_saved_regs){};
}

void arch_thread_info_regs(const struct pbl_thread *t, uint32_t regs[PBL_THREAD_REG_COUNT]) {
  (void)t;
  memset(regs, 0, sizeof(uint32_t) * PBL_THREAD_REG_COUNT);
}

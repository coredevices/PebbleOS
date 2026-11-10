/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/sem.h>

void pbl_sem_init(struct pbl_sem *s, uint32_t initial, uint32_t limit) {
  PBL_ASSERTN(limit > 0 && initial <= limit);
  *s = (struct pbl_sem)PBL_SEM_INITIALIZER(initial, limit);
  s->backend.count = initial;
}

void pbl_sem_deinit(struct pbl_sem *s) {
  (void)s;
}

int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  int rc = 0;
  unsigned int key = arch_irq_lock();
  if (s->backend.count > 0) {
    s->backend.count--;
  } else if (pbl_timeout_is_no_wait(timeout) || arch_in_isr()) {
    rc = -EBUSY;
  } else {
    // A give hands the token straight to the woken thread.
    rc = sched_block(&s->backend.waitq, timeout, key);
  }
  arch_irq_unlock(key);
  return rc;
}

void pbl_sem_give(struct pbl_sem *s) {
  unsigned int key = arch_irq_lock();
  struct pbl_thread *t = waitq_pop(&s->backend.waitq);
  if (t) {
    sched_wake(t, 0);
  } else if (s->backend.count < s->limit) {
    s->backend.count++;
  }
  arch_irq_unlock(key);
}

void pbl_sem_reset(struct pbl_sem *s) {
  unsigned int key = arch_irq_lock();
  s->backend.count = s->initial;
  arch_irq_unlock(key);
}

uint32_t pbl_sem_count(const struct pbl_sem *s) {
  return s->backend.count;
}

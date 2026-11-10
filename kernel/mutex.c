/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/mutex.h>

pbl_prio_t mutex_inherited_prio(const struct pbl_thread *t) {
  pbl_prio_t prio = t->backend.base_prio;

  for (const struct pbl_mutex *m = t->backend.held; m != nullptr; m = m->backend.next_held) {
    const struct pbl_thread *waiter = m->backend.waitq.head;
    if (waiter != nullptr && waiter->prio > prio) {
      prio = waiter->prio;
    }
  }
  return prio;
}

static void prv_prio_update(struct pbl_thread *t) {
  sched_prio_set(t, t->backend.base_prio, mutex_inherited_prio(t));
}

static void prv_own(struct pbl_mutex *m, struct pbl_thread *t) {
  m->owner = t;
  m->count = 1;
  m->backend.next_held = t->backend.held;
  t->backend.held = m;
}

static void prv_disown(struct pbl_mutex *m) {
  struct pbl_mutex **link = &m->owner->backend.held;

  while (*link != m) {
    link = &(*link)->backend.next_held;
  }
  *link = m->backend.next_held;
}

void pbl_mutex_init(struct pbl_mutex *m) {
  *m = (struct pbl_mutex)PBL_MUTEX_INITIALIZER;
}

void pbl_mutex_deinit(struct pbl_mutex *m) {
  PBL_ASSERTN(m->count == 0);
}

int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  PBL_ASSERTN(!arch_in_isr());
  int rc = 0;

  unsigned int key = arch_irq_lock();
  struct pbl_thread *me = pbl_cur;
  if (m->owner == nullptr) {
    prv_own(m, me);
  } else if (m->owner == me) {
    m->count++;
  } else if (pbl_timeout_is_no_wait(timeout)) {
    rc = -EBUSY;
  } else {
    if (me->prio > m->owner->prio) {
      sched_prio_set(m->owner, m->owner->backend.base_prio, me->prio);
    }
    // On success the previous owner handed the mutex over to us.
    rc = sched_block(&m->backend.waitq, timeout, key);
    if (rc != 0 && m->owner != nullptr) {
      // The owner no longer inherits from us.
      prv_prio_update(m->owner);
    }
  }
#if CONFIG_KERNEL_MUTEX_LOCK_LR
  if (rc == 0 && m->count == 1) {
    m->lock_lr = lr;
  }
#else
  (void)lr;
#endif
  arch_irq_unlock(key);
  return rc;
}

void pbl_mutex_unlock(struct pbl_mutex *m) {
  unsigned int key = arch_irq_lock();
  PBL_ASSERTN(m->owner == pbl_cur && m->count > 0);
  if (--m->count == 0) {
    struct pbl_thread *prev = m->owner;
    prv_disown(m);
    m->lock_lr = 0;

    struct pbl_thread *next = waitq_pop(&m->backend.waitq);
    if (next) {
      prv_own(m, next);
      sched_wake(next, 0);
    } else {
      m->owner = nullptr;
    }
    if (prev->prio != prev->backend.base_prio) {
      prv_prio_update(prev);
    }
  }
  arch_irq_unlock(key);
}

bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return m->count > 0 && m->owner == pbl_cur;
}

void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
  PBL_ASSERTN(pbl_mutex_is_owner(m) == held);
}

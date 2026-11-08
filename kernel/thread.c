/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <string.h>

#define STACK_FILL 0xa5a5a5a5u

static_assert(CONFIG_KERNEL_STACK_GUARD_SIZE == 0 ||
                  CONFIG_KERNEL_STACK_ALIGN % CONFIG_KERNEL_STACK_GUARD_SIZE == 0,
              "stacks must be aligned to their guard");

static uint32_t s_next_id;
static const struct pbl_mpu_region *s_default_regions[PBL_THREAD_MAX_MEM_REGIONS];

static void prv_regions_set(struct pbl_thread *t, const struct pbl_mpu_region *const *regions) {
  const struct pbl_mpu_region *merged[ARCH_THREAD_MPU_REGIONS] = {};
  for (unsigned int i = 0; i < PBL_THREAD_MAX_MEM_REGIONS; i++) {
    merged[i] = (regions != nullptr && regions[i] != nullptr) ? regions[i] : s_default_regions[i];
  }

#if CONFIG_KERNEL_STACK_GUARD_SIZE > 0
  const struct pbl_mpu_region stack_guard = {
    .base = (uintptr_t)t->stack - t->stack_guard_size,
    .size = t->stack_guard_size,
    .attr = PBL_MPU_NO_ACCESS | PBL_MPU_NORMAL_NC,
  };
  merged[THREAD_REGION_STACK_GUARD] = &stack_guard;

#ifdef CONFIG_USERSPACE
  const struct pbl_mpu_region syscall_guard = {
    .base = (uintptr_t)t->syscall_stack - CONFIG_KERNEL_STACK_GUARD_SIZE,
    .size = CONFIG_KERNEL_STACK_GUARD_SIZE,
    .attr = PBL_MPU_NO_ACCESS | PBL_MPU_NORMAL_NC,
  };
  if (t->syscall_stack != nullptr) {
    merged[THREAD_REGION_SYSCALL_GUARD] = &syscall_guard;
  }
#endif
#endif

  arch_thread_regions_set(t, merged);
}

int pbl_thread_create(struct pbl_thread *t, const struct pbl_thread_attr *attr) {
  const size_t guard_size =
      attr->user_stack ? CONFIG_KERNEL_USER_STACK_GUARD_SIZE : CONFIG_KERNEL_STACK_GUARD_SIZE;
  PBL_ASSERTN(attr->stack != nullptr && attr->stack_size >= 128 + guard_size);
#if CONFIG_KERNEL_STACK_GUARD_SIZE > 0
  PBL_ASSERTN(((uintptr_t)attr->stack % guard_size) == 0);
#endif
  PBL_ASSERTN(attr->prio <= PBL_PRIO_MAX);

  // A struct is only reusable once its previous thread is gone.
  PBL_ASSERTN(t->id == 0 || t->backend.state == PBL_THREAD_DEAD);

  memset(t, 0, sizeof(*t));
  t->id = ++s_next_id;
  strncpy(t->name, attr->name, sizeof(t->name) - 1);
  t->prio = attr->prio;
  t->privileged = attr->privileged;
  t->work_pool = attr->work_pool;
  t->stack = (uint8_t *)attr->stack + guard_size;
  t->stack_size = attr->stack_size - guard_size;
  t->stack_guard_size = guard_size;
#ifdef CONFIG_USERSPACE
  t->user_start = attr->user_start;
  t->user_end = attr->user_end;
  if (attr->syscall_stack != nullptr) {
    t->syscall_stack = (uint8_t *)attr->syscall_stack + CONFIG_KERNEL_STACK_GUARD_SIZE;
    t->syscall_stack_size = attr->syscall_stack_size - CONFIG_KERNEL_STACK_GUARD_SIZE;
  }
#endif

  uint32_t *words = t->stack;
  for (size_t i = 0; i < t->stack_size / sizeof(uint32_t); i++) {
    words[i] = STACK_FILL;
  }

  arch_thread_init(t, attr->entry, attr->arg);
  for (unsigned int i = 0; i < PBL_THREAD_MAX_MEM_REGIONS; i++) {
    t->own_regions |= attr->regions[i] != nullptr;
  }
  prv_regions_set(t, attr->regions);

  pbl_irq_lock();
  sched_thread_start(t);
  pbl_irq_unlock();
  return 0;
}

void pbl_thread_abort(struct pbl_thread *t) {
  pbl_irq_lock();
  bool self = (t == nullptr || t == pbl_cur);
  sched_thread_remove(self ? pbl_cur : t);
  if (!self) {
    arch_thread_aborted(t);
  }
  pbl_irq_unlock();
  if (self) {
    // The switch requested above takes over; this never returns.
    arch_thread_exit();
  }
}

void pbl_thread_suspend(struct pbl_thread *t) {
  pbl_irq_lock();
  sched_thread_suspend(t ? t : pbl_cur);
  pbl_irq_unlock();
}

void pbl_thread_resume(struct pbl_thread *t) {
  pbl_irq_lock();
  sched_thread_resume(t);
  pbl_irq_unlock();
}

struct pbl_thread *pbl_thread_current(void) {
  return pbl_cur;
}

struct pbl_thread *pbl_thread_idle(void) {
  return sched_idle_thread();
}

void pbl_thread_prio_set(struct pbl_thread *t, pbl_prio_t prio) {
  PBL_ASSERTN(prio <= PBL_PRIO_MAX);
  pbl_irq_lock();
  // Keep an inherited boost; only the base moves.
  pbl_prio_t effective = t->prio > t->backend.base_prio ? (prio > t->prio ? prio : t->prio) : prio;
  sched_prio_set(t, prio, effective);
  pbl_irq_unlock();
}

pbl_prio_t pbl_thread_prio_get(const struct pbl_thread *t) {
  return t->backend.base_prio;
}

enum pbl_thread_state pbl_thread_state(const struct pbl_thread *t) {
  return (enum pbl_thread_state)t->backend.state;
}

void pbl_thread_regions_default_set(const struct pbl_mpu_region *const *regions) {
  pbl_irq_lock();
  for (unsigned int i = 0; i < PBL_THREAD_MAX_MEM_REGIONS; i++) {
    s_default_regions[i] = regions[i];
  }
  for (struct pbl_thread *t = pbl_all_threads; t != nullptr; t = t->backend.all_next) {
    if (!t->own_regions) {
      prv_regions_set(t, nullptr);
    }
  }
  pbl_irq_unlock();
}

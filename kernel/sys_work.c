/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/util/misc.h>

#ifdef CONFIG_KERNEL_SYS_WORKQ
PBL_WORKQ_DEFINE(pbl_sys_workq);
static struct pbl_thread s_sys_thread;
PBL_THREAD_STACK_DEFINE(s_sys_stack, CONFIG_KERNEL_SYS_WORKQ_STACK_SIZE);

struct pbl_sys_work__heap_call {
  struct pbl_work work;
  pbl_work_call_fn_t fn;
  void *data;
};

static PBL_WORK_POOL_DEFINE(s_pool, CONFIG_KERNEL_SYS_WORKQ_CALLS);
//! The function behind a heap call, which is all the queue knows about.
static pbl_work_call_fn_t s_heap_fn;
static bool s_blocked;
#endif

#ifdef CONFIG_KERNEL_SYS_WORKQ_HIGH
PBL_WORKQ_DEFINE(pbl_sys_workq_high);
static struct pbl_thread s_sys_high_thread;
PBL_THREAD_STACK_DEFINE(s_sys_high_stack, CONFIG_KERNEL_SYS_WORKQ_HIGH_STACK_SIZE);

static PBL_WORK_POOL_DEFINE(s_high_pool, CONFIG_KERNEL_SYS_WORKQ_HIGH_CALLS);
#endif

[[maybe_unused]] static void prv_start(struct pbl_workq *q, struct pbl_thread *thread,
                                       const char *name, pbl_prio_t prio, void *stack,
                                       size_t stack_size) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = pbl_workq_thread,
    .arg = q,
    .prio = prio,
    .privileged = true,
    .stack = stack,
    .stack_size = stack_size,
  };

  // Set here too, as callers can ask for it before the thread first runs.
  q->thread = thread;
  PBL_ASSERTN(pbl_thread_create(thread, &attr) == 0);
}

void sys_workq_init(void) {
#ifdef CONFIG_KERNEL_SYS_WORKQ
  pbl_workq_set_boost_prio(&pbl_sys_workq, CONFIG_KERNEL_SYS_WORKQ_BOOST_PRIO);
  prv_start(&pbl_sys_workq, &s_sys_thread, "SysWorkq", CONFIG_KERNEL_SYS_WORKQ_PRIO, s_sys_stack,
            sizeof(s_sys_stack));
#endif
#ifdef CONFIG_KERNEL_SYS_WORKQ_HIGH
  prv_start(&pbl_sys_workq_high, &s_sys_high_thread, "SysWorkqHigh",
            CONFIG_KERNEL_SYS_WORKQ_HIGH_PRIO, s_sys_high_stack, sizeof(s_sys_high_stack));
#endif
}

#ifdef CONFIG_KERNEL_SYS_WORKQ
void *pbl_sys_work_current_fn(void) {
  return s_heap_fn ? (void *)s_heap_fn : pbl_workq_current_fn(&pbl_sys_workq);
}

[[noreturn]] static void prv_full(pbl_work_call_fn_t fn, uintptr_t lr) {
  PBL_PANIC_MSG("Work queue full: dropped %p from %p while running %p", (void *)fn, (void *)lr,
                pbl_sys_work_current_fn());
}

static void prv_heap_call_fn(struct pbl_work *work) {
  struct pbl_sys_work__heap_call *call =
      PBL_CONTAINER_OF(work, struct pbl_sys_work__heap_call, work);
  pbl_work_call_fn_t fn = call->fn;
  void *data = call->data;

  pbl_free(call);
  s_heap_fn = fn;
  fn(data);
  s_heap_fn = nullptr;
}

//! The queue runs its own calls, so it never waits for a slot: what does not fit goes on the heap.
static void prv_call_from_queue(pbl_work_call_fn_t fn, void *data, uintptr_t lr) {
  if (pbl_work_call(&pbl_sys_workq, &s_pool, fn, data, PBL_NO_WAIT) == 0) {
    return;
  }

  struct pbl_sys_work__heap_call *call = pbl_malloc(sizeof(*call));
  if (call == nullptr) {
    prv_full(fn, lr);
  }
  *call = (struct pbl_sys_work__heap_call){.fn = fn, .data = data};
  pbl_work_init(&call->work, prv_heap_call_fn);
  pbl_work_submit(&pbl_sys_workq, &call->work);
}

bool pbl_sys_work_call(pbl_work_call_fn_t fn, void *data) {
  const uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  if (s_blocked) {
    return false;
  }

  struct pbl_work_pool *own_pool = pbl_thread_current()->work_pool;
  if (own_pool != nullptr) {
    (void)pbl_work_call(&pbl_sys_workq, own_pool, fn, data, PBL_FOREVER);
  } else if (pbl_workq_is_current(&pbl_sys_workq)) {
    prv_call_from_queue(fn, data, lr);
  } else if (pbl_work_call(&pbl_sys_workq, &s_pool, fn, data,
                           PBL_MSEC(CONFIG_KERNEL_SYS_WORKQ_CALL_TIMEOUT_MS)) != 0) {
    prv_full(fn, lr);
  }
  return true;
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t fn, void *data) {
  const uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  if (s_blocked) {
    return false;
  }
  if (pbl_work_call(&pbl_sys_workq, &s_pool, fn, data, PBL_NO_WAIT) != 0) {
    prv_full(fn, lr);
  }
  return true;
}

bool pbl_sys_work_try_call(pbl_work_call_fn_t fn, void *data) {
  return !s_blocked && pbl_work_call(&pbl_sys_workq, &s_pool, fn, data, PBL_NO_WAIT) == 0;
}

void pbl_sys_work_block(bool block) {
  s_blocked = block;
}

size_t pbl_sys_work_num_free(void) {
  struct pbl_work_pool *own_pool = pbl_thread_current()->work_pool;

  return pbl_work_pool_num_free(own_pool != nullptr ? own_pool : &s_pool);
}
#endif

#ifdef CONFIG_KERNEL_SYS_WORKQ_HIGH
bool pbl_sys_work_call_high(pbl_work_call_fn_t fn, void *data) {
  return pbl_work_call(&pbl_sys_workq_high, &s_high_pool, fn, data,
                       PBL_MSEC(CONFIG_KERNEL_SYS_WORKQ_HIGH_CALL_TIMEOUT_MS)) == 0;
}

void pbl_sys_work_call_high_from_isr(pbl_work_call_fn_t fn, void *data) {
  (void)pbl_work_call(&pbl_sys_workq_high, &s_high_pool, fn, data, PBL_NO_WAIT);
}
#endif

#if UNITTEST
void sys_workq_reset_for_test(void) {
  memset(&s_sys_thread, 0, sizeof(s_sys_thread));
  memset(&s_sys_high_thread, 0, sizeof(s_sys_high_thread));
  pbl_workq_init(&pbl_sys_workq);
  pbl_workq_init(&pbl_sys_workq_high);
}

void sys_work_reset_for_test(void) {
  static struct pbl_work_call s_slots[CONFIG_KERNEL_SYS_WORKQ_CALLS];
  static struct pbl_work_call s_high_slots[CONFIG_KERNEL_SYS_WORKQ_HIGH_CALLS];

  pbl_work_pool_init(&s_pool, s_slots, CONFIG_KERNEL_SYS_WORKQ_CALLS);
  pbl_work_pool_init(&s_high_pool, s_high_slots, CONFIG_KERNEL_SYS_WORKQ_HIGH_CALLS);
  s_heap_fn = nullptr;
  s_blocked = false;
}
#endif

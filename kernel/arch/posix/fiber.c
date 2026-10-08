/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The thread state is host memory, not firmware heap.
#undef malloc
#undef free
#undef calloc
#include <stdlib.h>

#include <emscripten/emscripten.h>
#include <emscripten/fiber.h>
#include <string.h>

#include "pbl/kernel/idle.h"
#include "pbl/kernel/init.h"

#include "kernel.h"
#include "pbl_arch_posix.h"

// Kernel threads are Emscripten fibers on the host's single thread. The host
// runs the kernel until it idles (pbl_posix_run()), and delivers interrupts in
// between, as the posix pthread backend does whenever no kernel thread holds
// the CPU. A busy kernel also gives the host a turn now and then, so that the
// page stays responsive.

#define FIBER_C_STACK_SIZE     (128 * 1024)
#define FIBER_ASYNC_STACK_SIZE (64 * 1024)
#define HOST_TURN_MS           20

struct posix_thread {
  emscripten_fiber_t fiber;
  void *c_stack;
  void *async_stack;
  void (*entry)(void *);
  void *arg;
};

struct pbl_thread *pbl_cur;

static bool s_switch_pending;
static bool s_in_isr;

static emscripten_fiber_t s_host_fiber;
static uint8_t s_host_async_stack[FIBER_ASYNC_STACK_SIZE];
static struct posix_thread *s_boot;
static int (*s_entry)(void);
static emscripten_fiber_t *s_resume;
static double s_host_turn_ms;

// A thread that ended cannot free its own stacks; the next one to run does.
static struct posix_thread *s_zombie;

static void prv_free(struct posix_thread *pt) {
  free(pt->c_stack);
  free(pt->async_stack);
  free(pt);
}

static void prv_reap(void) {
  if (s_zombie != NULL) {
    prv_free(s_zombie);
    s_zombie = NULL;
  }
}

static struct posix_thread *prv_new(void (*entry)(void *), void *arg) {
  struct posix_thread *pt = calloc(1, sizeof(*pt));
  KERNEL_ASSERT(pt != NULL);
  pt->c_stack = malloc(FIBER_C_STACK_SIZE);
  pt->async_stack = malloc(FIBER_ASYNC_STACK_SIZE);
  KERNEL_ASSERT(pt->c_stack != NULL && pt->async_stack != NULL);
  pt->entry = entry;
  pt->arg = arg;
  return pt;
}

static void prv_fiber_main(void *arg) {
  struct posix_thread *pt = arg;
  prv_reap();
  pt->entry(pt->arg);
  pbl_thread_abort(NULL);
  KERNEL_ASSERT(false);
}

static void prv_fiber_init(struct posix_thread *pt) {
  emscripten_fiber_init(&pt->fiber, prv_fiber_main, pt, pt->c_stack, FIBER_C_STACK_SIZE,
                        pt->async_stack, FIBER_ASYNC_STACK_SIZE);
}

// Hands the CPU back to the host, which resumes from here.
static void prv_host_turn(emscripten_fiber_t *from) {
  s_resume = from;
  emscripten_fiber_swap(from, &s_host_fiber);
  s_host_turn_ms = emscripten_get_now();
  prv_reap();
}

static void prv_switch(void) {
  s_switch_pending = false;
  struct pbl_thread *prev = pbl_cur;
  struct pbl_thread *next = sched_switch_in();
  if (next == prev) {
    return;
  }
  struct posix_thread *pt = prev->backend.arch.pt;
  if (prev->backend.state == PBL_THREAD_DEAD) {
    prev->backend.arch.pt = NULL;
    s_zombie = pt;
  }
  emscripten_fiber_swap(&pt->fiber, &next->backend.arch.pt->fiber);
  prv_reap();
}

void arch_init(void) {
}

void arch_thread_init(struct pbl_thread *t, void (*entry)(void *), void *arg) {
  struct posix_thread *pt = prv_new(entry, arg);
  prv_fiber_init(pt);
  t->backend.arch.pt = pt;
}

void arch_thread_regions_set(struct pbl_thread *t, const MpuRegion *const *regions) {
}

void arch_start(void) {
  irq_reset();
  // The boot fiber is done; it is never resumed.
  emscripten_fiber_swap(&s_boot->fiber, &pbl_cur->backend.arch.pt->fiber);
  KERNEL_ASSERT(false);
  for (;;) {
  }
}

void arch_switch_request(void) {
  s_switch_pending = true;
}

void arch_thread_exit(void) {
  KERNEL_ASSERT(false);
  for (;;) {
  }
}

void arch_thread_aborted(struct pbl_thread *t) {
  prv_free(t->backend.arch.pt);
  t->backend.arch.pt = NULL;
}

bool arch_in_isr(void) {
  return s_in_isr;
}

void arch_irq_disable(void) {
}

void arch_irq_enable(void) {
  if (s_in_isr || !pbl_kernel_is_started()) {
    return;
  }
  if (emscripten_get_now() - s_host_turn_ms > HOST_TURN_MS) {
    prv_host_turn(&pbl_cur->backend.arch.pt->fiber);
  }
  if (s_switch_pending) {
    prv_switch();
  }
}

void arch_thread_saved_regs(const struct pbl_thread *t, struct pbl_thread_saved_regs *regs) {
  *regs = (struct pbl_thread_saved_regs){0};
}

void arch_thread_info_regs(const struct pbl_thread *t, uint32_t regs[PBL_THREAD_REG_COUNT]) {
  memset(regs, 0, sizeof(uint32_t) * PBL_THREAD_REG_COUNT);
}

void arch_idle(pbl_tick_t max_ticks) {
  pbl_soc_idle(max_ticks);
}

static void prv_boot_main(void *arg) {
  prv_reap();
  pbl_soc_early_init();
  s_entry();
  KERNEL_ASSERT(false);
}

void pbl_posix_start(int (*entry)(void)) {
  s_entry = entry;
}

void pbl_posix_run(void) {
  if (s_entry == NULL) {
    return;
  }
  if (s_resume == NULL) {
    emscripten_fiber_init_from_current_context(&s_host_fiber, s_host_async_stack,
                                               sizeof(s_host_async_stack));
    s_boot = prv_new(prv_boot_main, NULL);
    emscripten_fiber_init(&s_boot->fiber, prv_boot_main, NULL, s_boot->c_stack, FIBER_C_STACK_SIZE,
                          s_boot->async_stack, FIBER_ASYNC_STACK_SIZE);
    s_resume = &s_boot->fiber;
  }
  emscripten_fiber_swap(&s_host_fiber, s_resume);
}

void pbl_posix_irq_run(void (*isr)(void *), void *arg) {
  s_in_isr = true;
  isr(arg);
  s_in_isr = false;
}

void pbl_posix_cpu_wait(uint64_t timeout_us) {
  prv_host_turn(&pbl_cur->backend.arch.pt->fiber);
}

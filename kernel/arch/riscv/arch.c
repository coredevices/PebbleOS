/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "riscv.h"

#include <inttypes.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>

#include <kernel.h>

struct pbl_thread *pbl_cur;

uint32_t arch_riscv_started;
uint32_t arch_riscv_isr_nesting;

static_assert(offsetof(struct pbl_thread, backend.sp) == 0, "saved SP must be first");

void arch_cur_set(struct pbl_thread *t) {
  pbl_cur = t;
}

// ---- threads ----------------------------------------------------------------

static void prv_thread_return(void) {
  pbl_thread_abort(nullptr);
}

// The first switch to the thread returns into @p entry, interrupts unmasked, at the threads' level
void arch_thread_init(struct pbl_thread *t, void (*entry)(void *), void *arg) {
  uintptr_t top = ((uintptr_t)t->stack + t->stack_size) & ~7U;
  struct arch_riscv_context *ctx =
      (struct arch_riscv_context *)(top - sizeof(struct arch_riscv_context));

  memset(ctx, 0, sizeof(*ctx));
  ctx->frame.ra = (uint32_t)prv_thread_return;
  ctx->frame.a0 = (uint32_t)arg;
  ctx->mepc = (uint32_t)entry;
  ctx->mcause = ARCH_MCAUSE_INTERRUPT | ARCH_MCAUSE_MPP_M | ARCH_MCAUSE_MPIE;
  t->backend.sp = ctx;
}

void arch_thread_aborted(struct pbl_thread *t) {
  (void)t;
}

void arch_thread_exit(void) {
  // The switch requested by the abort takes over as soon as we get here.
  for (;;) {
    __asm volatile("wfi");
  }
}

// ---- interrupts -------------------------------------------------------------

bool arch_in_isr(void) {
  return mcu_state_is_isr();
}

bool mcu_state_are_interrupts_enabled(void) {
  return !arch_irq_is_locked();
}

static unsigned int s_heap_key;
static bool s_heap_locked;

void arch_heap_lock(void) {
  unsigned int key = arch_irq_lock();

  if (key != 0U) {
    s_heap_key = key;
    s_heap_locked = true;
  }
}

void arch_heap_unlock(void) {
  if (s_heap_locked) {
    s_heap_locked = false;
    arch_irq_unlock(s_heap_key);
  }
}

// Taken once interrupts are unmasked, or the last ISR returns
void arch_switch_request(void) {
  soc_riscv_switch_raise();
  if (!arch_irq_is_locked()) {
    arch_switch_wait();
  }
}

[[noreturn]] PBL_USED void arch_riscv_fault(uint32_t mcause, uintptr_t mepc, uintptr_t mtval,
                                            uintptr_t ra) {
  const struct pbl_fault_info info = {
    .reason = (mcause & ARCH_MCAUSE_CODE_MSK) == 2U ? PBL_FAULT_USAGE : PBL_FAULT_BUS,
    .pc = mepc,
    .lr = ra,
    .addr = mtval,
  };

  PBL_LOG_FAULT("Fault: mcause 0x%" PRIx32 " mepc 0x%" PRIxPTR " mtval 0x%" PRIxPTR, mcause, mepc,
                mtval);
  pbl_fault_thread_handler(&info);
}

// ---- start and idle ---------------------------------------------------------

void arch_early_init(void) {
#ifndef CONFIG_RISCV_SOC_CYCLE_COUNTER
  // Count cycles, for the busy waits
  arch_csr_clear(ARCH_CSR_MCOUNTINHIBIT, 1U);
#endif
}

void arch_init(void) {
}

void arch_start(void) {
  // The thread switch takes over from here, dropping this context
  arch_switch_request();
  arch_irq_unlock(ARCH_MSTATUS_MIE);
  for (;;) {
  }
}

void arch_idle(pbl_tick_t max_ticks) {
  unsigned int key = arch_irq_lock();

  if (sched_idle_confirm()) {
    idle_enter(max_ticks);
  }
  arch_irq_unlock(key);
}

// The core cannot reset more than itself: the SoC does, with SOC_HAS_RESET
[[noreturn]] void arch_reset(void) {
  for (;;) {
    __asm volatile("wfi");
  }
}

// ---- introspection ----------------------------------------------------------

void arch_thread_saved_regs(const struct pbl_thread *t, struct pbl_thread_saved_regs *regs) {
  const struct arch_riscv_context *ctx = t->backend.sp;

  *regs = (struct pbl_thread_saved_regs){.pc = ctx->mepc, .lr = ctx->frame.ra};
}

// a0-a5 and ra take the place of r0-r5 and lr
void arch_thread_info_regs(const struct pbl_thread *t, uint32_t regs[PBL_THREAD_REG_COUNT]) {
  const struct arch_riscv_context *ctx = t->backend.sp;

  regs[PBL_THREAD_REG_R0] = ctx->frame.a0;
  regs[PBL_THREAD_REG_R1] = ctx->frame.a1;
  regs[PBL_THREAD_REG_R2] = ctx->frame.a2;
  regs[PBL_THREAD_REG_R3] = ctx->frame.a3;
  regs[PBL_THREAD_REG_R4] = ctx->frame.a4;
  regs[PBL_THREAD_REG_R5] = ctx->frame.a5;
  regs[PBL_THREAD_REG_R8] = ctx->s0;
  regs[PBL_THREAD_REG_R9] = ctx->s1;
  regs[PBL_THREAD_REG_SP] = (uint32_t)(uintptr_t)(ctx + 1) + (ctx->frame.mepc & 1U) * 4U;
  regs[PBL_THREAD_REG_LR] = ctx->frame.ra;
  regs[PBL_THREAD_REG_PC] = ctx->mepc;
  regs[PBL_THREAD_REG_XPSR] = ctx->mcause;
}

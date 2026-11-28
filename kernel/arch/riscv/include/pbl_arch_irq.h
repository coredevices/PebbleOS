/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

#include <pbl_arch_csr.h>
#include <soc_irqs.h>
#include <soc_riscv.h>

// CLIC levels, the most urgent last; level 0 is the threads', which no interrupt preempts. Level 1
// switches threads, so that a switch only ever interrupts a thread, and the others are the lines'.
#define ARCH_IRQ_LEVELS (1U << CONFIG_RISCV_CLIC_INTCTL_BITS)

#define ARCH_IRQ_PRIO_LOWEST (ARCH_IRQ_LEVELS - 3U)
#define ARCH_IRQ_PRIO_KERNEL ARCH_IRQ_PRIO_LOWEST

struct arch_irq_prio {
  uint16_t irq;
  uint8_t prio;
};

#define ARCH_IRQ_HANDLER(_n)  ARCH_IRQ_HANDLER_(_n)
#define ARCH_IRQ_HANDLER_(_n) pbl_isr_##_n
#define ARCH_IRQ_PRIO(_n)     ARCH_IRQ_PRIO_(_n)
#define ARCH_IRQ_PRIO_(_n)    pbl_irq_prio_##_n

#define ARCH_IRQ_DECLARE(_n, _name, _prio, _flags)                                                 \
  static_assert((_prio) >= 0 && (_prio) <= ARCH_IRQ_PRIO_LOWEST, _name ": priority out of range"); \
  static_assert(!((_flags) & PBL_IRQ_ZERO_LATENCY), _name ": no zero-latency interrupts");         \
  static const struct arch_irq_prio ARCH_IRQ_PRIO(_n)                                              \
      PBL_ITERABLE_SECTION(const struct arch_irq_prio, irq_prio, "") = {                           \
        .irq = (_n),                                                                               \
        .prio = (_prio),                                                                           \
  };                                                                                               \
  void ARCH_IRQ_HANDLER(_n)(void)

#define ARCH_IRQ_CONNECT(_n, _name, _prio, _isr, _arg, _flags) \
  ARCH_IRQ_DECLARE(_n, _name, _prio, _flags);                  \
  void ARCH_IRQ_HANDLER(_n)(void) {                            \
    _isr(_arg);                                                \
  }                                                            \
  static_assert(1, "")

#define ARCH_IRQ_DIRECT(_n, _name, _prio, _flags) \
  ARCH_IRQ_DECLARE(_n, _name, _prio, _flags);     \
  void ARCH_IRQ_HANDLER(_n)(void)

// The kernel lock masks every interrupt: none is more urgent than the kernel's own
static PBL_ALWAYS_INLINE unsigned int arch_irq_lock(void) {
  unsigned long mstatus;

  __asm volatile("csrrci %0, mstatus, %1" : "=r"(mstatus) : "i"(ARCH_MSTATUS_MIE) : "memory");

  return mstatus & ARCH_MSTATUS_MIE;
}

//! Interrupt handlers running, counted by the trap entry.
extern uint32_t arch_riscv_isr_nesting;

// A thread switch is taken a few cycles after it can be: until then, a thread would run on past
// where it blocks. Wait for it, unless in a handler, which it does not preempt.
static PBL_ALWAYS_INLINE void arch_switch_wait(void) {
  while (soc_riscv_switch_pending() && arch_riscv_isr_nesting == 0U) {
  }
}

static PBL_ALWAYS_INLINE void arch_irq_unlock(unsigned int key) {
  arch_csr_set(ARCH_CSR_MSTATUS, key);
  if (key != 0U) {
    arch_switch_wait();
  }
}

static PBL_ALWAYS_INLINE bool arch_irq_is_locked(void) {
  return (arch_csr_read(ARCH_CSR_MSTATUS) & ARCH_MSTATUS_MIE) == 0U;
}

static PBL_ALWAYS_INLINE unsigned int arch_irq_disable_all(void) {
  return arch_irq_lock();
}

static PBL_ALWAYS_INLINE void arch_irq_restore_all(unsigned int key) {
  arch_irq_unlock(key);
}

static PBL_ALWAYS_INLINE bool arch_irq_window_pending(unsigned int key) {
  (void)key;
  return false;
}

static PBL_ALWAYS_INLINE void arch_irq_window(void) {
}

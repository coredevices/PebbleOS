/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <soc_irqs.h>

#include <pbl/kernel/compiler.h>

struct arch_irq_prio {
  uint16_t irq;
  uint8_t prio;
};

#define ARCH_IRQ_PRIO_MAX_SYSCALL (CONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL >> (8 - __NVIC_PRIO_BITS))

#define ARCH_IRQ_HANDLER(_n)  ARCH_IRQ_HANDLER_(_n)
#define ARCH_IRQ_HANDLER_(_n) pbl_isr_##_n
#define ARCH_IRQ_PRIO(_n)     ARCH_IRQ_PRIO_(_n)
#define ARCH_IRQ_PRIO_(_n)    pbl_irq_prio_##_n

#define ARCH_IRQ_DECLARE(_n, _irq, _prio, _flags)                                               \
  _Static_assert((_n) == (_irq), #_irq ": number differs from soc_irqs.h");                     \
  _Static_assert((_prio) >= 0 && (_prio) < (1 << __NVIC_PRIO_BITS),                             \
                 #_irq ": priority out of range");                                              \
  _Static_assert(((_flags) & PBL_IRQ_ZERO_LATENCY) || (_prio) >= ARCH_IRQ_PRIO_MAX_SYSCALL,     \
                 #_irq ": priority above PBL_IRQ_PRIO_MAX_SYSCALL needs PBL_IRQ_ZERO_LATENCY"); \
  PBL_USED PBL_SECTION(".pbl_irq_prio") static const struct arch_irq_prio ARCH_IRQ_PRIO(_n) = { \
    .irq = (_n),                                                                                \
    .prio = (_prio),                                                                            \
  };                                                                                            \
  void ARCH_IRQ_HANDLER(_n)(void)

#define ARCH_IRQ_CONNECT(_n, _irq, _prio, _isr, _arg, _flags) \
  ARCH_IRQ_DECLARE(_n, _irq, _prio, _flags);                  \
  void ARCH_IRQ_HANDLER(_n)(void) {                           \
    _isr(_arg);                                               \
  }                                                           \
  _Static_assert(1, "")

#define ARCH_IRQ_DIRECT(_n, _irq, _prio, _flags) \
  ARCH_IRQ_DECLARE(_n, _irq, _prio, _flags);     \
  void ARCH_IRQ_HANDLER(_n)(void)

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#define ARCH_IRQ_HANDLER(_n)  ARCH_IRQ_HANDLER_(_n)
#define ARCH_IRQ_HANDLER_(_n) pbl_isr_##_n

#define ARCH_IRQ_CONNECT(_n, _name, _prio, _isr, _arg, _flags) \
  void ARCH_IRQ_HANDLER(_n)(void);                             \
  void ARCH_IRQ_HANDLER(_n)(void) {                            \
    _isr(_arg);                                                \
  }                                                            \
  static_assert(1, "")

#define ARCH_IRQ_DIRECT(_n, _name, _prio, _flags) \
  void ARCH_IRQ_HANDLER(_n)(void);                \
  void ARCH_IRQ_HANDLER(_n)(void)

// Hosts run nothing from RAM in place of the flash: nothing to mask or let through
static inline unsigned int arch_irq_disable_all(void) {
  return 0U;
}

static inline void arch_irq_restore_all(unsigned int key) {
  (void)key;
}

static inline bool arch_irq_window_pending(unsigned int key) {
  (void)key;
  return false;
}

static inline void arch_irq_window(void) {
}

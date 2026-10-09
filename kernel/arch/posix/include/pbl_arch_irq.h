/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define ARCH_IRQ_HANDLER(_n)  ARCH_IRQ_HANDLER_(_n)
#define ARCH_IRQ_HANDLER_(_n) pbl_isr_##_n

#define ARCH_IRQ_CONNECT(_n, _irq, _prio, _isr, _arg, _flags) \
  void ARCH_IRQ_HANDLER(_n)(void);                            \
  void ARCH_IRQ_HANDLER(_n)(void) {                           \
    _isr(_arg);                                               \
  }                                                           \
  static_assert(1, "")

#define ARCH_IRQ_DIRECT(_n, _irq, _prio, _flags) \
  void ARCH_IRQ_HANDLER(_n)(void);               \
  void ARCH_IRQ_HANDLER(_n)(void)

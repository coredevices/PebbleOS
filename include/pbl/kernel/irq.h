/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! NVIC priority of the kernel's own interrupts (tick, context switch).
#define PBL_IRQ_PRIO_KERNEL CONFIG_KERNEL_IRQ_PRIO_KERNEL
//! Highest NVIC priority (lowest value) from which kernel calls are allowed.
#define PBL_IRQ_PRIO_MAX_SYSCALL CONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL

//! Masks interrupts up to PBL_IRQ_PRIO_MAX_SYSCALL. Nestable; usable from
//! threads and ISRs.
void pbl_irq_lock(void);
void pbl_irq_unlock(void);

bool pbl_in_isr(void);
bool pbl_irq_is_locked(void);

//! A line of the SoC interrupt controller.
typedef uint16_t pbl_irq_t;

//! The number of SoC IRQ @p line from soc_irqs.h, e.g. PBL_IRQN(I2C1).
#define PBL_IRQN(line) ((pbl_irq_t)PBL_SOC_IRQN_##line)

//! The ISR runs above PBL_IRQ_PRIO_MAX_SYSCALL: pbl_irq_lock() does not mask
//! it, and it must not call into the kernel.
#define PBL_IRQ_ZERO_LATENCY (1U << 0)

#include <pbl_arch_irq.h>

//! Binds SoC IRQ @p line to `isr(arg)` at build time; @p arg may be empty.
//! @p prio is in controller units (0 = most urgent) and is programmed by
//! pbl_irq_init(). Expands where the SoC's CMSIS header is included.
#define PBL_IRQ_CONNECT(line, prio, isr, arg, flags) \
  ARCH_IRQ_CONNECT(PBL_SOC_IRQN_##line, line##_IRQn, prio, isr, arg, flags)

//! Like PBL_IRQ_CONNECT(), with the handler body following the macro.
#define PBL_IRQ_DIRECT(line, prio, flags) \
  ARCH_IRQ_DIRECT(PBL_SOC_IRQN_##line, line##_IRQn, prio, flags)

//! Installs the vector table and programs the priority of every connected
//! IRQ. Called once at boot, before any IRQ is enabled.
void pbl_irq_init(void);

void pbl_irq_enable(pbl_irq_t irq);
void pbl_irq_disable(pbl_irq_t irq);
bool pbl_irq_is_enabled(pbl_irq_t irq);
void pbl_irq_set_pending(pbl_irq_t irq);
void pbl_irq_clear_pending(pbl_irq_t irq);

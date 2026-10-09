/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup kernel_irq Interrupts
 * @ingroup kernel
 * @brief Interrupt locking and build-time interrupt binding.
 *
 * SoC interrupt lines are bound to their handlers at build time; there is no runtime handler
 * registration and the vector table lives in flash. A line is named after its
 * `PBL_SOC_IRQN_<line>` define in the SoC's @c soc_irqs.h, which matches the CMSIS
 * `<line>_IRQn` name. Binding a line twice fails to link. pbl_irq_init() programs the priority
 * of every bound line at boot, so drivers only enable and disable them:
 *
 * @code{.c}
 * PBL_IRQ_CONNECT(I2C1, 5, i2c_irq_handler, &s_i2c1_bus, 0);
 *
 * PBL_IRQ_DIRECT(AON, 0, PBL_IRQ_ZERO_LATENCY) {
 *   // handler body; must not call the kernel
 * }
 *
 * void i2c_init(void) {
 *   pbl_irq_enable(PBL_IRQN(I2C1));
 * }
 * @endcode
 *
 * pbl_irq_lock() masks every interrupt that may call the kernel, and is how kernel objects
 * protect their state:
 *
 * @code{.c}
 * pbl_irq_lock();
 * s_pending |= flag;
 * pbl_irq_unlock();
 * @endcode
 * @{
 */

/**
 * @brief NVIC priority of the kernel's own interrupts (tick, context switch).
 *
 * Raw 8-bit priority register value, from @c CONFIG_KERNEL_IRQ_PRIO_KERNEL.
 */
#define PBL_IRQ_PRIO_KERNEL CONFIG_KERNEL_IRQ_PRIO_KERNEL
/**
 * @brief Most urgent NVIC priority from which kernel calls are allowed.
 *
 * Raw 8-bit priority register value (lower is more urgent), from
 * @c CONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL. pbl_irq_lock() masks this level and every less urgent
 * one.
 */
#define PBL_IRQ_PRIO_MAX_SYSCALL CONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL

/**
 * @brief Mask interrupts up to @ref PBL_IRQ_PRIO_MAX_SYSCALL.
 *
 * Nestable; balance each call with pbl_irq_unlock(). Usable from threads and ISRs. Interrupts
 * flagged @ref PBL_IRQ_ZERO_LATENCY are not masked.
 */
void pbl_irq_lock(void);
/** @brief Undo one pbl_irq_lock(), unmasking interrupts at the outermost one. */
void pbl_irq_unlock(void);

/**
 * @brief Check whether the caller runs in interrupt context.
 *
 * @return true in an ISR or exception handler.
 */
bool pbl_in_isr(void);
/**
 * @brief Check whether pbl_irq_lock() is in effect.
 *
 * @return true if interrupts are locked.
 */
bool pbl_irq_is_locked(void);

/** @brief Line of the SoC interrupt controller. */
typedef uint16_t pbl_irq_t;

/**
 * @brief Number of a SoC interrupt line from @c soc_irqs.h.
 *
 * For example, @c PBL_IRQN(I2C1) is @c PBL_SOC_IRQN_I2C1.
 *
 * @param line Line name.
 */
#define PBL_IRQN(line) ((pbl_irq_t)PBL_SOC_IRQN_##line)

/**
 * @brief Flag for an ISR that runs above @ref PBL_IRQ_PRIO_MAX_SYSCALL.
 *
 * pbl_irq_lock() does not mask it, and it must not call into the kernel. Required for any
 * priority more urgent than @ref PBL_IRQ_PRIO_MAX_SYSCALL, which is a build error otherwise.
 */
#define PBL_IRQ_ZERO_LATENCY (1U << 0)

#include <pbl_arch_irq.h>

/**
 * @brief Bind a SoC interrupt line to a handler at build time.
 *
 * The handler is called as @c isr(arg) with whatever type @p isr takes; an empty @p arg calls
 * @c isr(). Expands to a definition, at file scope, where the SoC's CMSIS header is included.
 *
 * @param line Line name, as in @c soc_irqs.h.
 * @param prio Priority in controller units, 0 being the most urgent.
 * @param isr Handler function.
 * @param arg Argument passed to @p isr, may be empty.
 * @param flags 0 or @ref PBL_IRQ_ZERO_LATENCY.
 */
#define PBL_IRQ_CONNECT(line, prio, isr, arg, flags) \
  ARCH_IRQ_CONNECT(PBL_SOC_IRQN_##line, line##_IRQn, prio, isr, arg, flags)

/**
 * @brief Bind a SoC interrupt line to the handler body that follows the macro.
 *
 * @param line Line name, as in @c soc_irqs.h.
 * @param prio Priority in controller units, 0 being the most urgent.
 * @param flags 0 or @ref PBL_IRQ_ZERO_LATENCY.
 */
#define PBL_IRQ_DIRECT(line, prio, flags) \
  ARCH_IRQ_DIRECT(PBL_SOC_IRQN_##line, line##_IRQn, prio, flags)

/**
 * @brief Install the vector table and program the priority of every bound line.
 *
 * Called once at boot, before any interrupt is enabled.
 */
void pbl_irq_init(void);

/**
 * @brief Enable an interrupt line.
 *
 * @param irq Line, see @ref PBL_IRQN.
 */
void pbl_irq_enable(pbl_irq_t irq);
/**
 * @brief Disable an interrupt line.
 *
 * @param irq Line, see @ref PBL_IRQN.
 */
void pbl_irq_disable(pbl_irq_t irq);
/**
 * @brief Check whether an interrupt line is enabled.
 *
 * @param irq Line, see @ref PBL_IRQN.
 * @return true if enabled.
 */
bool pbl_irq_is_enabled(pbl_irq_t irq);
/**
 * @brief Mark an interrupt line pending, so its handler runs once enabled and unmasked.
 *
 * @param irq Line, see @ref PBL_IRQN.
 */
void pbl_irq_set_pending(pbl_irq_t irq);
/**
 * @brief Clear the pending state of an interrupt line.
 *
 * @param irq Line, see @ref PBL_IRQN.
 */
void pbl_irq_clear_pending(pbl_irq_t irq);

/** @} */

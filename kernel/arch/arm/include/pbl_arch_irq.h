/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>
#include <pbl/util/math.h>

#include <devicetree/regs.h>
#include <pbl_arch_mmio.h>
#include <soc_irqs.h>

#define ARCH_IRQ_PRIO_BITS  PBL_DT_PROP_NVIC_ARM_NUM_IRQ_PRIORITY_BITS
#define ARCH_IRQ_PRIO_SHIFT (8U - ARCH_IRQ_PRIO_BITS)

// Controller levels: the zero-latency interrupts, if any, take the most urgent one, as radio
// schedulers require, then the faults, so that they are taken in an ISR or with the kernel
// locked, then all the others, priority 0 being the first level the kernel lock masks. A fault
// in a zero-latency handler escalates to a HardFault.
#ifdef CONFIG_KERNEL_ZERO_LATENCY_IRQS
#define ARCH_IRQ_PRIO_ZLI    0U
#define ARCH_IRQ_PRIO_FAULT  1U
#define ARCH_IRQ_PRIO_OFFSET 2U
#else
#define ARCH_IRQ_PRIO_ZLI    1U
#define ARCH_IRQ_PRIO_FAULT  0U
#define ARCH_IRQ_PRIO_OFFSET 1U
#endif
#define ARCH_IRQ_PRIO_LOWEST ((1U << ARCH_IRQ_PRIO_BITS) - 1U - ARCH_IRQ_PRIO_OFFSET)

// As the priority registers and BASEPRI take them
#define ARCH_IRQ_PRIO_FAULT_HW (ARCH_IRQ_PRIO_FAULT << ARCH_IRQ_PRIO_SHIFT)
#define ARCH_IRQ_PRIO_HW(_p)   (((_p) + ARCH_IRQ_PRIO_OFFSET) << ARCH_IRQ_PRIO_SHIFT)
#define ARCH_IRQ_PRIO_LOCK_HW  ARCH_IRQ_PRIO_HW(0U)
#define ARCH_IRQ_PRIO_KERNEL   (0xFFU & ~((1U << ARCH_IRQ_PRIO_SHIFT) - 1U))

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
  static_assert(!((_flags) & PBL_IRQ_ZERO_LATENCY) || ARCH_IRQ_PRIO_OFFSET > ARCH_IRQ_PRIO_ZLI,    \
                _name ": PBL_IRQ_ZERO_LATENCY needs CONFIG_KERNEL_ZERO_LATENCY_IRQS");             \
  static const struct arch_irq_prio ARCH_IRQ_PRIO(_n)                                              \
      PBL_ITERABLE_SECTION(const struct arch_irq_prio, irq_prio, "") = {                           \
        .irq = (_n),                                                                               \
        .prio = ((_flags) & PBL_IRQ_ZERO_LATENCY) ? ARCH_IRQ_PRIO_ZLI                              \
                                                  : (_prio) + ARCH_IRQ_PRIO_OFFSET,                \
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

#define ARCH_NVIC_ISER(i) (0xE000E100UL + ((i) * 4U))
#define ARCH_NVIC_ISPR(i) (0xE000E200UL + ((i) * 4U))
#define ARCH_NVIC_IPR(n)  (0xE000E400UL + (n))
#define ARCH_NVIC_WORDS   PBL_DIV_ROUND_UP(CONFIG_NUM_IRQS, 32U)

static PBL_ALWAYS_INLINE unsigned int arch_irq_disable_all(void) {
  unsigned int primask;

  __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");

  return primask;
}

static PBL_ALWAYS_INLINE void arch_irq_restore_all(unsigned int key) {
  __asm volatile("msr primask, %0" ::"r"(key) : "memory");
}

static PBL_ALWAYS_INLINE uint32_t arch_irq_basepri(void) {
  uint32_t basepri;

  __asm volatile("mrs %0, basepri" : "=r"(basepri));

  return basepri;
}

// Masks the interrupts allowed to call into the kernel, never unmasking any.
static PBL_ALWAYS_INLINE unsigned int arch_irq_lock(void) {
  unsigned int key;

  __asm volatile("mrs %0, basepri\n\tmsr basepri_max, %1\n\tisb"
                 : "=&r"(key)
                 : "r"(ARCH_IRQ_PRIO_LOCK_HW)
                 : "memory");

  return key;
}

// The barrier has a pending interrupt, a context switch included, taken here.
static PBL_ALWAYS_INLINE void arch_irq_unlock(unsigned int key) {
  __asm volatile("msr basepri, %0\n\tisb" ::"r"(key) : "memory");
}

static PBL_ALWAYS_INLINE bool arch_irq_is_locked(void) {
  return arch_irq_basepri() != 0U;
}

// The more urgent of the kernel's own priority and the caller's mask
static PBL_ALWAYS_INLINE uint32_t arch_irq_window_level(uint32_t basepri) {
  return (basepri != 0U && basepri < ARCH_IRQ_PRIO_KERNEL) ? basepri : ARCH_IRQ_PRIO_KERNEL;
}

static PBL_ALWAYS_INLINE bool arch_irq_window_pending(unsigned int key) {
  const uint32_t level = arch_irq_window_level(arch_irq_basepri());

  if (key != 0U) {
    return false;
  }

  for (uint32_t i = 0U; i < ARCH_NVIC_WORDS; i++) {
    uint32_t pending = arch_readl(ARCH_NVIC_ISPR(i)) & arch_readl(ARCH_NVIC_ISER(i));

    while (pending != 0U) {
      const uint32_t n = (i * 32U) + (uint32_t)PBL_CTZ(pending);

      if (arch_readb(ARCH_NVIC_IPR(n)) < level) {
        return true;
      }

      pending &= pending - 1U;
    }
  }

  return false;
}

static PBL_ALWAYS_INLINE void arch_irq_window(void) {
  const uint32_t basepri = arch_irq_basepri();

  __asm volatile("msr basepri, %0\n\tcpsie i\n\tisb 0xf\n\tcpsid i\n\tmsr basepri, %1" ::"r"(
                     arch_irq_window_level(basepri)),
                 "r"(basepri)
                 : "memory");
}

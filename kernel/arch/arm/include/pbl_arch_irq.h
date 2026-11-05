/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>
#include <soc_irqs.h>

struct arch_irq_prio {
  uint16_t irq;
  uint8_t prio;
};

#define ARCH_IRQ_PRIO_MAX_SYSCALL \
  (CONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL >> (8 - CONFIG_NUM_IRQ_PRIO_BITS))

#define ARCH_IRQ_HANDLER(_n)  ARCH_IRQ_HANDLER_(_n)
#define ARCH_IRQ_HANDLER_(_n) pbl_isr_##_n
#define ARCH_IRQ_PRIO(_n)     ARCH_IRQ_PRIO_(_n)
#define ARCH_IRQ_PRIO_(_n)    pbl_irq_prio_##_n

#define ARCH_IRQ_DECLARE(_n, _name, _prio, _flags)                                              \
  static_assert((_prio) >= 0 && (_prio) < (1 << CONFIG_NUM_IRQ_PRIO_BITS),                      \
                _name ": priority out of range");                                               \
  static_assert(((_flags) & PBL_IRQ_ZERO_LATENCY) || (_prio) >= ARCH_IRQ_PRIO_MAX_SYSCALL,      \
                _name ": priority above PBL_IRQ_PRIO_MAX_SYSCALL needs PBL_IRQ_ZERO_LATENCY");  \
  PBL_USED PBL_SECTION(".pbl_irq_prio") static const struct arch_irq_prio ARCH_IRQ_PRIO(_n) = { \
    .irq = (_n),                                                                                \
    .prio = (_prio),                                                                            \
  };                                                                                            \
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
#define ARCH_NVIC_WORDS   ((CONFIG_NUM_IRQS + 31U) / 32U)

// Priority of the kernel's own interrupts as the NVIC stores it, unimplemented bits clear
#define ARCH_IRQ_PRIO_KERNEL_HW \
  (CONFIG_KERNEL_IRQ_PRIO_KERNEL & ~((1U << (8U - CONFIG_NUM_IRQ_PRIO_BITS)) - 1U))

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

// The more urgent of the kernel's own priority and the caller's mask
static PBL_ALWAYS_INLINE uint32_t arch_irq_window_level(uint32_t basepri) {
  return (basepri != 0U && basepri < ARCH_IRQ_PRIO_KERNEL_HW) ? basepri : ARCH_IRQ_PRIO_KERNEL_HW;
}

static PBL_ALWAYS_INLINE bool arch_irq_window_pending(unsigned int key) {
  const uint32_t level = arch_irq_window_level(arch_irq_basepri());

  if (key != 0U) {
    return false;
  }

  for (uint32_t i = 0U; i < ARCH_NVIC_WORDS; i++) {
    uint32_t pending = arch_readl(ARCH_NVIC_ISPR(i)) & arch_readl(ARCH_NVIC_ISER(i));

    while (pending != 0U) {
      const uint32_t n = (i * 32U) + (uint32_t)__builtin_ctz(pending);

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

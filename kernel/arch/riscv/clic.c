/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "riscv.h"

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/section.h>
#include <pbl/util/listify.h>

#include <kernel.h>
#include <pbl_arch_mmio.h>

PBL_ITERABLE_SECTION_DECLARE(const struct arch_irq_prio, irq_prio);

void arch_irq_spurious(void);
void arch_irq_spurious(void) {
  PBL_ASSERTN(false);
}

#define ARCH_HANDLER_DECLARE(n) PBL_WEAK PBL_ALIAS("arch_irq_spurious") void pbl_isr_##n(void);
PBL_LISTIFY(CONFIG_NUM_IRQS, ARCH_HANDLER_DECLARE)

void arch_riscv_trap(void);

// The CLIC vectors every line in hardware, through mtvt: all of them to the trap entry, which
// takes the line from mcause
PBL_USED PBL_ALIGNED(
    64) void (*const arch_riscv_vectors[CONFIG_RISCV_CLIC_IRQ_OFFSET + CONFIG_NUM_IRQS])(void) = {
  [0 ... CONFIG_RISCV_CLIC_IRQ_OFFSET + CONFIG_NUM_IRQS - 1] = arch_riscv_trap,
};

PBL_USED void (*const arch_riscv_irq_table[])(void) = {
#define ARCH_HANDLER_ENTRY(n) pbl_isr_##n,
  PBL_LISTIFY(CONFIG_NUM_IRQS, ARCH_HANDLER_ENTRY)
};

// The level in the bits the CLIC implements, the lower ones read as ones
static uint8_t prv_ctl(unsigned int level) {
  const unsigned int shift = 8U - CONFIG_RISCV_CLIC_INTCTL_BITS;

  return (uint8_t)((level << shift) | ((1U << shift) - 1U));
}

static void prv_ctl_set(pbl_irq_t irq, uint8_t ctl) {
  const uintptr_t reg = CLICINT(irq);

  arch_writel((arch_readl(reg) & ~(0xffU << CLICINT_CTL_POS)) | ((uint32_t)ctl << CLICINT_CTL_POS),
              reg);
}

void pbl_irq_init(void) {
  PBL_ITERABLE_SECTION_FOREACH(const struct arch_irq_prio, irq_prio, p) {
    prv_ctl_set(p->irq, prv_ctl(ARCH_IRQ_LEVELS - 1U - p->prio));
  }

  prv_ctl_set(RISCV_SWITCH_IRQ, prv_ctl(1U));
  pbl_irq_enable(RISCV_SWITCH_IRQ);
}

void pbl_irq_enable(pbl_irq_t irq) {
  arch_writel(arch_readl(CLICINT(irq)) | CLICINT_IE, CLICINT(irq));
}

void pbl_irq_disable(pbl_irq_t irq) {
  arch_writel(arch_readl(CLICINT(irq)) & ~CLICINT_IE, CLICINT(irq));
  __asm volatile("fence io, io" ::: "memory");
}

bool pbl_irq_is_enabled(pbl_irq_t irq) {
  return (arch_readl(CLICINT(irq)) & CLICINT_IE) != 0U;
}

void pbl_irq_set_pending(pbl_irq_t irq) {
  arch_writel(arch_readl(CLICINT(irq)) | CLICINT_IP, CLICINT(irq));
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
  arch_writel(arch_readl(CLICINT(irq)) & ~CLICINT_IP, CLICINT(irq));
}

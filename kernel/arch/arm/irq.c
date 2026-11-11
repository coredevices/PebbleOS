/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <string.h>

#include <pbl/kernel/compiler.h>

#include <kernel.h>

extern const void *const arch_vector_table[];
extern const struct arch_irq_prio __pbl_irq_prio_start[];
extern const struct arch_irq_prio __pbl_irq_prio_end[];

#ifdef CONFIG_ARCH_VECTOR_TABLE_RAM
#define NUM_VECTORS  (16 + CONFIG_NUM_IRQS)
#define VECTORS_SIZE (NUM_VECTORS * 4U)

// VTOR wants the table aligned to its size rounded up to a power of two.
#define SMEAR1(x)     ((x) | ((x) >> 1))
#define SMEAR2(x)     (SMEAR1(x) | (SMEAR1(x) >> 2))
#define SMEAR4(x)     (SMEAR2(x) | (SMEAR2(x) >> 4))
#define SMEAR8(x)     (SMEAR4(x) | (SMEAR4(x) >> 8))
#define VECTORS_ALIGN (SMEAR8(VECTORS_SIZE - 1U) + 1U)

static const void *s_ram_vectors[NUM_VECTORS] PBL_ALIGNED(VECTORS_ALIGN);
#endif

void pbl_irq_init(void) {
#ifdef CONFIG_ARCH_VECTOR_TABLE_RAM
  memcpy(s_ram_vectors, arch_vector_table, sizeof(s_ram_vectors));
  arch_writel((uint32_t)s_ram_vectors, ARM_SCB_VTOR);
#else
  arch_writel((uint32_t)arch_vector_table, ARM_SCB_VTOR);
#endif

  // All priority bits are preemption priority.
  arch_writel(
      (arch_readl(ARM_SCB_AIRCR) & ~(ARM_SCB_AIRCR_VECTKEY_MSK | ARM_SCB_AIRCR_PRIGROUP_MSK)) |
          ARM_SCB_AIRCR_VECTKEY | (3UL << ARM_SCB_AIRCR_PRIGROUP_POS),
      ARM_SCB_AIRCR);

  for (const struct arch_irq_prio *p = __pbl_irq_prio_start; p < __pbl_irq_prio_end; p++) {
    arch_writeb((uint8_t)(p->prio << (8U - CONFIG_NUM_IRQ_PRIO_BITS)), ARCH_NVIC_IPR(p->irq));
  }
}

#define IRQ_WORD(irq) ((uint32_t)(irq) / 32U)
#define IRQ_BIT(irq)  (1UL << ((uint32_t)(irq) % 32U))

void pbl_irq_enable(pbl_irq_t irq) {
  arch_writel(IRQ_BIT(irq), ARCH_NVIC_ISER(IRQ_WORD(irq)));
}

void pbl_irq_disable(pbl_irq_t irq) {
  arch_writel(IRQ_BIT(irq), ARM_NVIC_ICER(IRQ_WORD(irq)));
  arm_dsb();
  arm_isb();
}

bool pbl_irq_is_enabled(pbl_irq_t irq) {
  return (arch_readl(ARCH_NVIC_ISER(IRQ_WORD(irq))) & IRQ_BIT(irq)) != 0U;
}

void pbl_irq_set_pending(pbl_irq_t irq) {
  arch_writel(IRQ_BIT(irq), ARCH_NVIC_ISPR(IRQ_WORD(irq)));
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
  arch_writel(IRQ_BIT(irq), ARM_NVIC_ICPR(IRQ_WORD(irq)));
}

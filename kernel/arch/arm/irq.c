/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/compiler.h>

#include <cmsis_core.h>
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
  SCB->VTOR = (uint32_t)s_ram_vectors;
#else
  SCB->VTOR = (uint32_t)arch_vector_table;
#endif

  // All priority bits are preemption priority.
  NVIC_SetPriorityGrouping(3);

  for (const struct arch_irq_prio *p = __pbl_irq_prio_start; p < __pbl_irq_prio_end; p++) {
    NVIC_SetPriority((IRQn_Type)p->irq, p->prio);
  }
}

void pbl_irq_enable(pbl_irq_t irq) {
  NVIC_EnableIRQ((IRQn_Type)irq);
}

void pbl_irq_disable(pbl_irq_t irq) {
  NVIC_DisableIRQ((IRQn_Type)irq);
}

bool pbl_irq_is_enabled(pbl_irq_t irq) {
  return NVIC_GetEnableIRQ((IRQn_Type)irq) != 0U;
}

void pbl_irq_set_pending(pbl_irq_t irq) {
  NVIC_SetPendingIRQ((IRQn_Type)irq);
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
  NVIC_ClearPendingIRQ((IRQn_Type)irq);
}

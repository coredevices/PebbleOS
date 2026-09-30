/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <cmsis_core.h>

#include "kernel.h"

extern const void *const arch_vector_table[];
extern const struct arch_irq_prio __pbl_irq_prio_start[];
extern const struct arch_irq_prio __pbl_irq_prio_end[];

void pbl_irq_init(void) {
  SCB->VTOR = (uint32_t)arch_vector_table;

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

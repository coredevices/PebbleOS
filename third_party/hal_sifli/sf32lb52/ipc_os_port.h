/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bf0_hal.h"

void vPortEnterCritical(void);
void vPortExitCritical(void);

static inline int os_interrupt_disable(void) {
  vPortEnterCritical();
  return 0;
}

static inline void os_interrupt_enable(int mask) {
  vPortExitCritical();
}

#define os_interrupt_enter()
#define os_interrupt_exit()

#define os_interrupt_start(irq_number, priority, sub_priority) \
  do {                                                         \
    HAL_NVIC_SetPriority(irq_number, priority, sub_priority);  \
    HAL_NVIC_EnableIRQ(irq_number);                            \
  } while (0)

#define os_interrupt_stop(irq_number) HAL_NVIC_DisableIRQ(irq_number)

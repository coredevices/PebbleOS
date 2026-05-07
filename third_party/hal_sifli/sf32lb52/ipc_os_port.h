/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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

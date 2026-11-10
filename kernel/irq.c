/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

static uint32_t s_nesting;
static unsigned int s_key;

void pbl_irq_lock(void) {
  unsigned int key = arch_irq_lock();

  if (s_nesting++ == 0) {
    s_key = key;
  }
}

void pbl_irq_unlock(void) {
  PBL_ASSERTN(s_nesting > 0);
  if (--s_nesting == 0) {
    arch_irq_unlock(s_key);
  }
}

void irq_reset_for_test(void) {
  s_nesting = 0;
}

bool pbl_in_isr(void) {
  return arch_in_isr();
}

bool pbl_irq_is_locked(void) {
  return arch_irq_is_locked();
}

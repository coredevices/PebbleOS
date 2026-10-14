/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>

PBL_WEAK void pbl_irq_lock(void) {
}

PBL_WEAK void pbl_irq_unlock(void) {
}

PBL_WEAK bool pbl_irq_is_locked(void) {
  return false;
}

PBL_WEAK bool pbl_in_isr(void) {
  return false;
}

PBL_WEAK void sys_psleep(int millis) {
}

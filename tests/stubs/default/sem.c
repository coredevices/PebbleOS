/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/sem.h>

PBL_WEAK void pbl_sem_init(struct pbl_sem *s, uint32_t initial, uint32_t limit) {
}

PBL_WEAK void pbl_sem_deinit(struct pbl_sem *s) {
}

PBL_WEAK int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  return 0;
}

PBL_WEAK void pbl_sem_give(struct pbl_sem *s) {
}

PBL_WEAK void pbl_sem_reset(struct pbl_sem *s) {
}

PBL_WEAK uint32_t pbl_sem_count(const struct pbl_sem *s) {
  return 0;
}

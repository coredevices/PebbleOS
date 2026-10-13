/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>

PBL_WEAK void pbl_mutex_init(struct pbl_mutex *m) {
}

PBL_WEAK void pbl_mutex_deinit(struct pbl_mutex *m) {
}

PBL_WEAK int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  return 0;
}

PBL_WEAK void pbl_mutex_unlock(struct pbl_mutex *m) {
}

PBL_WEAK bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return true;
}

PBL_WEAK void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
}

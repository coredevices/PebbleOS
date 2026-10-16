/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/als.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK const struct pbl_als *const ALS = nullptr;

PBL_WEAK int pbl_als_read(const struct pbl_als *als, uint32_t *level) {
  return -ENODEV;
}

PBL_WEAK void pbl_als_prime(const struct pbl_als *als) {
}

PBL_WEAK void pbl_als_release(const struct pbl_als *als) {
}

PBL_WEAK void pbl_als_suspend(const struct pbl_als *als) {
}

PBL_WEAK void pbl_als_resume(const struct pbl_als *als) {
}

PBL_WEAK bool pbl_als_has_lux(const struct pbl_als *als) {
  return false;
}

PBL_WEAK uint32_t pbl_als_to_lux(const struct pbl_als *als, uint32_t level) {
  return level;
}

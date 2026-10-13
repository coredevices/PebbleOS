/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

PBL_WEAK void *app_state_get_rand_ptr(void) {
  return nullptr;
}
PBL_WEAK void *worker_state_get_rand_ptr(void) {
  return nullptr;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

PBL_WEAK void bt_lock(void) {
}

PBL_WEAK void bt_unlock(void) {
}

PBL_WEAK void bt_lock_assert_held(bool is_held) {
}

PBL_WEAK bool bt_lock_is_held(void) {
  return false;
}

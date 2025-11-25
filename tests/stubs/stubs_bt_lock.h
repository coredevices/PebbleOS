/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void bt_lock(void) {
}

void bt_unlock(void) {
}

void bt_lock_assert_held(bool is_held) {
}

bool bt_lock_is_held(void) {
  return false;
}

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <shell/normal/button_lock.h>

bool PBL_WEAK button_lock_is_locked(void) {
  return false;
}

void PBL_WEAK button_lock_handle_prefs_changed(void) {
}

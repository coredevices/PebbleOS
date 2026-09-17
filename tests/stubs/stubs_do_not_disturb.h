/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/kernel/compiler.h"

bool PBL_WEAK do_not_disturb_is_active(void) {
  return false;
}

void PBL_WEAK do_not_disturb_init(void) {
}

void PBL_WEAK do_not_disturb_manual_toggle_with_dialog(void) {
}

void PBL_WEAK do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSource source) {
}

void PBL_WEAK do_not_disturb_handle_pref_synced(void) {
}

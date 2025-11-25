/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/do_not_disturb.h"
#include "util/attributes.h"

bool WEAK do_not_disturb_is_active(void) {
  return false;
}

void WEAK do_not_disturb_init(void) {}

void WEAK do_not_disturb_manual_toggle_with_dialog(void) {}

void WEAK do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSource source) {}

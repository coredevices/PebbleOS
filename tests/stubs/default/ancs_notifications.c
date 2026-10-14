/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/notifications/ancs/ancs_notifications.h>

PBL_WEAK void ancs_notifications_enable_bulk_action_mode(bool enable) {
}

PBL_WEAK bool ancs_notifications_is_bulk_action_mode_enabled(void) {
  return false;
}

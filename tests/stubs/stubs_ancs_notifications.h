/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/ancs/ancs_notifications.h"

void ancs_notifications_enable_bulk_action_mode(bool enable) { }

bool ancs_notifications_is_bulk_action_mode_enabled(void) {
  return false;
}

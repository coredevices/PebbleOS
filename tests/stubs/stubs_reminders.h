/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"
#include "util/attributes.h"

status_t WEAK reminders_update_timer(void) {
  return S_SUCCESS;
}

status_t WEAK reminders_init(void) {
  return S_SUCCESS;
}

void WEAK reminders_handle_reminder_updated(const Uuid *reminder_id) {
  return;
}

bool WEAK reminders_can_snooze(Reminder *reminder) {
  return false;
}

status_t WEAK reminders_snooze(Reminder *reminder) {
  return S_SUCCESS;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/reminders.h"
#include "system/status_codes.h"
#include "pbl/kernel/compiler.h"

status_t PBL_WEAK reminders_update_timer(void) {
  return S_SUCCESS;
}

status_t PBL_WEAK reminders_init(void) {
  return S_SUCCESS;
}

void PBL_WEAK reminders_handle_reminder_updated(const Uuid *reminder_id) {
  return;
}

bool PBL_WEAK reminders_can_snooze(Reminder *reminder) {
  return false;
}

status_t PBL_WEAK reminders_snooze(Reminder *reminder) {
  return S_SUCCESS;
}

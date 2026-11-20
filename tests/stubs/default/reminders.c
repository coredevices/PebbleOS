/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/timeline/reminders.h>

int PBL_WEAK reminders_update_timer(void) {
  return 0;
}

int PBL_WEAK reminders_init(void) {
  return 0;
}

void PBL_WEAK reminders_handle_reminder_updated(const Uuid *reminder_id) {
  return;
}

bool PBL_WEAK reminders_can_snooze(Reminder *reminder) {
  return false;
}

int PBL_WEAK reminders_snooze(Reminder *reminder) {
  return 0;
}

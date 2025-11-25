/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

bool s_empty = true;

void fake_reminder_db_set_empty(bool empty) {
  s_empty = empty;
}

bool reminder_db_is_empty() {
  return s_empty;
}


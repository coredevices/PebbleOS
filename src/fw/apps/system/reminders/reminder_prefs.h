/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#define PREF_KEY_REMINDER_APP "remindersApp"

typedef enum ReminderAppState {
  ReminderAppState_NotEnabled = 0,
  ReminderAppState_NotConfigured = 1,
  ReminderAppState_Enabled = 2,
  ReminderAppStateCount
} ReminderAppState;

typedef struct PACKED SerializedReminderAppPrefs {
  uint8_t appState;  // actually enum ReminderAppState
} SerializedReminderAppPrefs;

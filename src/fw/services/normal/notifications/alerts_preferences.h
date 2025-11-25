/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef enum FirstUseSource {
  FirstUseSourceManualDNDActionMenu = 0,
  FirstUseSourceManualDNDSettingsMenu,
  FirstUseSourceSmartDND,
  FirstUseSourceDismiss
} FirstUseSource;

typedef enum MuteBitfield {
  MuteBitfield_None     = 0b00000000,
  MuteBitfield_Always   = 0b01111111,
  MuteBitfield_Weekdays = 0b00111110,
  MuteBitfield_Weekends = 0b01000001,
} MuteBitfield;

//! Checks whether a given "first use" dialog has been shown and sets it as complete
//! @param source The "first use" bit to check
//! @return true if the dialog has already been shown, false otherwise
bool alerts_preferences_check_and_set_first_use_complete(FirstUseSource source);

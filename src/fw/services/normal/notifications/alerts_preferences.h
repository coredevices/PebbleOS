/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>

#include "alerts_private.h"

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

typedef enum {
  DndNotificationModeHide = 0,
  DndNotificationModeShow = 1,
} DndNotificationMode;

//! Set notification display mode when DND is active
//! @param mode The display mode (Show or Hide)
void alerts_preferences_dnd_set_show_notifications(DndNotificationMode mode);

//! @return The notification display mode when DND is active
DndNotificationMode alerts_preferences_dnd_get_show_notifications(void);

//! Checks whether a given "first use" dialog has been shown and sets it as complete
//! @param source The "first use" bit to check
//! @return true if the dialog has already been shown, false otherwise
bool alerts_preferences_check_and_set_first_use_complete(FirstUseSource source);

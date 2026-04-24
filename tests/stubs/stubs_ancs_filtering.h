/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/blob_db/ios_notif_pref_db.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "util/attributes.h"

uint8_t WEAK ancs_filtering_get_mute_type(const iOSNotifPrefs *app_notif_prefs) {
  return MuteBitfield_None;
}

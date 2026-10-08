/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/ios_notif_pref_db.h>
#include <pbl/services/notifications/alerts_preferences.h>

uint8_t PBL_WEAK ancs_filtering_get_mute_type(const iOSNotifPrefs *app_notif_prefs) {
  return MuteBitfield_None;
}

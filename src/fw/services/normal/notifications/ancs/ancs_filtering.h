/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "services/normal/blob_db/ios_notif_pref_db.h"

//! Updates the entry in the notif pref db for a given app
//! @param app_notif_prefs The existing prefs for the app we want to update. These prefs could be
//! updated in the process
//! @param app_id ID of the app we're recording
//! @param display_name Display name of the app we're recording
//! @param title Title attribute from the notification associated with the app we're recording
//! (fallback in the event that we don't have a display name such as in the case of Apple Pay)
void ancs_filtering_record_app(iOSNotifPrefs **app_notif_prefs,
                               const ANCSAttribute *app_id,
                               const ANCSAttribute *display_name,
                               const ANCSAttribute *title);

//! Returns true if a given app is muted for the current day
//! @param app_notif_prefs Prefs for the given app loaded from the notif pref db
//! @return true if the given app is muted
bool ancs_filtering_is_muted(const iOSNotifPrefs *app_notif_prefs);

//! Returns the mute type for an app
//! @param app_notif_prefs Prefs for the given app loaded from the notif pref db
//! @return MuteBitfield which is the mute type of the app
uint8_t ancs_filtering_get_mute_type(const iOSNotifPrefs *app_notif_prefs);

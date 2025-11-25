/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "services/normal/blob_db/ios_notif_pref_db.h"

bool nexmo_is_reauth_sms(const ANCSAttribute *app_id, const ANCSAttribute *message);

//! Adds the reauth msg to the notif prefs so the phone can start the reauth process
void nexmo_handle_reauth_sms(uint32_t uid,
                             const ANCSAttribute *app_id,
                             const ANCSAttribute *message,
                             iOSNotifPrefs *existing_notif_prefs);

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/ancs.h>
#include <pbl/services/blob_db/ios_notif_pref_db.h>

/**
 * @defgroup services_notifications_ancs_nexmo Nexmo reauthentication
 * @ingroup services_notifications_ancs
 * @brief Handles the SMS check-in code used to reauthenticate SMS replies.
 * @{
 */

/**
 * @brief Check whether a notification is a reauthentication SMS.
 *
 * @param app_id App identifier attribute.
 * @param message Message attribute.
 * @return true for a Messages notification containing the Pebble check-in code.
 */
bool nexmo_is_reauth_sms(const struct pbl_bt_ancs_attr *app_id,
                         const struct pbl_bt_ancs_attr *message);

/**
 * @brief Forward a reauthentication SMS to the phone and dismiss it.
 *
 * Stores the message in the app's notification prefs as an auth code attribute, which syncs it to
 * the phone, then performs the ANCS negative action so the user never sees it.
 *
 * @param uid ANCS UID of the SMS notification.
 * @param app_id App identifier attribute.
 * @param message Message attribute.
 * @param existing_notif_prefs Existing prefs of the Messages app.
 */
void nexmo_handle_reauth_sms(uint32_t uid, const struct pbl_bt_ancs_attr *app_id,
                             const struct pbl_bt_ancs_attr *message,
                             iOSNotifPrefs *existing_notif_prefs);

/** @} */

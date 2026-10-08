/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <comm/ble/kernel_le_client/ancs/ancs_types.h>
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
bool nexmo_is_reauth_sms(const ANCSAttribute *app_id, const ANCSAttribute *message);

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
void nexmo_handle_reauth_sms(uint32_t uid, const ANCSAttribute *app_id,
                             const ANCSAttribute *message, iOSNotifPrefs *existing_notif_prefs);

/** @} */

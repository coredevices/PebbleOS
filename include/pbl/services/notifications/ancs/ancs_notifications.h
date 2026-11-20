/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

#include <pbl/bluetooth/ancs.h>
#include <pbl/services/timeline/item.h>

/**
 * @defgroup services_notifications_ancs ANCS notifications
 * @ingroup services_notifications
 * @brief Turns notifications received over the Apple Notification Center Service into items.
 *
 * The ANCS client hands over the fetched notification and app attributes. Each app seen is
 * recorded in the iOS notification preferences database, which the phone syncs to add actions,
 * colors, mute schedules and filtering rules. Muted and filtered notifications are dropped,
 * incoming calls go to the phone service, duplicates are ignored, and the rest are stored as
 * notifications (see @ref services_notifications).
 * @{
 */

/** @brief The attributes got of a notification, in the order they are asked for. */
enum ancs_notif_attr {
  ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL,
  ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL,
  ANCS_NOTIF_ATTR_APP_ID,
  ANCS_NOTIF_ATTR_TITLE,
  ANCS_NOTIF_ATTR_SUBTITLE,
  ANCS_NOTIF_ATTR_MESSAGE,
  ANCS_NOTIF_ATTR_DATE,
  ANCS_NOTIF_ATTR_COUNT,
};

/** @brief The attributes got of the app of a notification. */
enum ancs_app_attr {
  ANCS_APP_ATTR_DISPLAY_NAME,
  ANCS_APP_ATTR_COUNT,
};

/**
 * @name Longest values taken
 *
 * The phone cuts the title, the subtitle and the message to theirs.
 * @{
 */
#define ANCS_APP_ID_MAX_LENGTH 60
/* Long enough for filtering rules to match text near the end of verbose titles. */
#define ANCS_TITLE_MAX_LENGTH    128
#define ANCS_SUBTITLE_MAX_LENGTH 40
#define ANCS_MESSAGE_MAX_LENGTH  200
#define ANCS_DATE_LENGTH         15
#define ANCS_ACTION_MAX_LENGTH   10
/** @} */

/**
 * @brief Handle a notification fetched from ANCS.
 *
 * @param notif The notification, as the phone told of it.
 * @param recovered Whether it predates the session, seen right after reconnecting: it may
 * have been missed while disconnected.
 * @param notif_attributes Notification attributes, indexed by enum ancs_notif_attr.
 * @param app_attributes App attributes, indexed by enum ancs_app_attr.
 */
void ancs_notifications_handle_message(const struct pbl_bt_ancs_notif *notif, bool recovered,
                                       const struct pbl_bt_ancs_attr *const *notif_attributes,
                                       const struct pbl_bt_ancs_attr *const *app_attributes);

/**
 * @brief Handle the removal of a notification from the iOS notification center.
 *
 * The stored notification is marked dismissed, and an incoming call UI is hidden.
 *
 * @param notif The removed notification, as the phone told of it.
 */
void ancs_notifications_handle_notification_removed(const struct pbl_bt_ancs_notif *notif);

/**
 * @brief Tell the user that iOS refuses to share notifications with this watch.
 *
 * Called when iOS keeps rejecting Control Point writes. Posts a notification explaining how to
 * turn on notification sharing.
 */
void ancs_notifications_handle_access_denied(void);

/**
 * @brief Record that the ANCS session went away.
 *
 * Notifications posted after this point are recovered when iOS re-sends them as pre-existing on
 * reconnection.
 */
void ancs_notifications_handle_session_lost(void);

/** @} */

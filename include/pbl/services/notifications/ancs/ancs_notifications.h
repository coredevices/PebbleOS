/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "pbl/services/timeline/item.h"

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

/**
 * @brief Handle a notification fetched from ANCS.
 *
 * @param uid ANCS UID of the notification.
 * @param properties ANCS properties (category, flags, iOS version).
 * @param notif_attributes Notification attributes, indexed by FetchedNotifAttributeIndex.
 * @param app_attributes App attributes, indexed by FetchedAppAttributeIndex.
 */
void ancs_notifications_handle_message(uint32_t uid, ANCSProperty properties,
                                       ANCSAttribute **notif_attributes,
                                       ANCSAttribute **app_attributes);

/**
 * @brief Handle the removal of a notification from the iOS notification center.
 *
 * Only honored on iOS 9 and later: the stored notification is marked dismissed, and an incoming
 * call UI is hidden.
 *
 * @param ancs_uid ANCS UID of the removed notification.
 * @param properties ANCS properties.
 */
void ancs_notifications_handle_notification_removed(uint32_t ancs_uid, ANCSProperty properties);

/**
 * @brief Tell the user that iOS refuses to share notifications with this watch.
 *
 * Called when iOS keeps rejecting Control Point writes. Posts a notification explaining how to
 * turn on notification sharing.
 */
void ancs_notifications_handle_access_denied(void);

/** @} */

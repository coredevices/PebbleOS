/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ancs_notifications_util.h"

#include <comm/ble/kernel_le_client/ancs/ancs_types.h>
#include <pbl/services/blob_db/ios_notif_pref_db.h>

/**
 * @defgroup services_notifications_ancs_ancs_item ANCS items
 * @ingroup services_notifications_ancs
 * @brief Builds timeline items from ANCS notifications.
 * @{
 */

/**
 * @brief Create a timeline item from ANCS data.
 *
 * Strings that reach the ANCS length limit are ellipsized. Actions are, in order, the ANCS
 * positive and negative actions followed by the actions from @p notif_prefs.
 *
 * @param notif_attributes Notification attributes, indexed by FetchedNotifAttributeIndex.
 * @param app_attributes App attributes (the display name), indexed by FetchedAppAttributeIndex.
 * @param app_metadata Icon and color of the app.
 * @param notif_prefs iOS notification prefs of the app, may be NULL.
 * @param timestamp Time the notification occurred.
 * @param properties ANCS properties (category, flags).
 * @return New item allocated on the calling task's heap (free with timeline_item_destroy()), or
 *         NULL if out of memory. Header fields other than the timestamp are left for the caller.
 */
TimelineItem *ancs_item_create_and_populate(ANCSAttribute *notif_attributes[],
                                            ANCSAttribute *app_attributes[],
                                            const ANCSAppMetadata *app_metadata,
                                            iOSNotifPrefs *notif_prefs, time_t timestamp,
                                            ANCSProperty properties);

/**
 * @brief Replace the dismiss action of an item with an ANCS negative action.
 *
 * The item's buffer is reallocated; pointers into the old one become invalid.
 *
 * @param[in,out] item Item to update. Nothing happens if it has no dismiss action.
 * @param uid ANCS UID of the notification whose negative action is used.
 * @param attr_action_neg Negative action label from that notification.
 */
void ancs_item_update_dismiss_action(TimelineItem *item, uint32_t uid,
                                     const ANCSAttribute *attr_action_neg);

/** @} */

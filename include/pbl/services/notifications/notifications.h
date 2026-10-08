/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/timeline/item.h>

/**
 * @defgroup services_notifications Notifications
 * @ingroup services
 * @brief Notification storage, alerting policy and Do Not Disturb.
 *
 * A notification is a TimelineItem of type TimelineItemTypeNotification (see
 * @ref services_timeline_item). Notifications arrive from the phone through the Notifs BlobDB, from
 * ANCS on iOS (@ref services_notifications_ancs) or are created on the watch. They are kept in a
 * flash file (@ref services_notifications_notification_storage) that is wiped at boot, and every
 * change is announced to the UI with a @c PEBBLE_SYS_NOTIFICATION_EVENT.
 *
 * Whether and how the user is alerted is decided by the alerts service
 * (@ref services_notifications_alerts), which combines the user preferences
 * (@ref services_notifications_alerts_preferences) with the Do Not Disturb state
 * (@ref services_notifications_do_not_disturb).
 *
 * Posting a notification created on the watch:
 *
 * @code{.c}
 * AttributeList attr_list = {0};
 * attribute_list_add_cstring(&attr_list, AttributeIdTitle, "Title");
 * attribute_list_add_cstring(&attr_list, AttributeIdBody, "Body text");
 *
 * AttributeList dismiss_attrs = {0};
 * attribute_list_add_cstring(&dismiss_attrs, AttributeIdTitle, "Dismiss");
 * TimelineItemActionGroup action_group = {
 *   .num_actions = 1,
 *   .actions = (TimelineItemAction[]){
 *     {.id = 0, .type = TimelineItemActionTypeDismiss, .attr_list = dismiss_attrs},
 *   },
 * };
 *
 * TimelineItem *item = timeline_item_create_with_attributes(
 *     rtc_get_time(), 0, TimelineItemTypeNotification, LayoutIdNotification, &attr_list,
 *     &action_group);
 * attribute_list_destroy_list(&attr_list);
 * attribute_list_destroy_list(&dismiss_attrs);
 * if (item) {
 *   notifications_add_notification(item);
 *   timeline_item_destroy(item);
 * }
 * @endcode
 *
 * Removing it again:
 *
 * @code{.c}
 * notification_storage_remove(&id);
 * notifications_handle_notification_removed(&id);
 * @endcode
 * @{
 */

/** @brief Outcome of an action invoked on a timeline item. */
typedef enum {
  /** The action succeeded. */
  ActionResultTypeSuccess,
  /** The action failed. */
  ActionResultTypeFailure,
  /** The phone needs the user to pick a follow-up action from the result's action group. */
  ActionResultTypeChaining,
  /** The phone asks the watch to start a reply. */
  ActionResultTypeDoResponse,
  /** The action succeeded and the ANCS notification should also be dismissed. */
  ActionResultTypeSuccessANCSDismiss,
} ActionResultType;

/**
 * @brief Result of an action, delivered in a @c PEBBLE_SYS_NOTIFICATION_EVENT.
 *
 * Allocated on the kernel heap as a single block with the attributes and actions following the
 * struct; the event loop frees it after dispatch.
 */
typedef struct {
  /** Id of the item the action was invoked on. */
  Uuid id;
  /** Outcome of the action. */
  ActionResultType type;
  /** Result attributes, typically a message (title) and a large icon. */
  AttributeList attr_list;
  /** Follow-up actions, used with ActionResultTypeChaining. */
  TimelineItemActionGroup action_group;
} PebbleSysNotificationActionResult;

/**
 * @brief Initialize the notifications service.
 *
 * Initializes notification storage, which discards all stored notifications.
 */
void notifications_init(void);

/**
 * @brief Post the result of an invoked action.
 *
 * @param action_result Result allocated on the kernel heap, or NULL when there is no result to
 *                      show. Ownership passes to the event loop.
 */
void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result);

/**
 * @brief Announce that a notification was added to storage.
 *
 * @param notification_id Id allocated on the kernel heap. Ownership passes to the event loop.
 */
void notifications_handle_notification_added(Uuid *notification_id);

/**
 * @brief Announce that a stored notification was acted upon or updated on the phone.
 *
 * @param notification_id Id allocated on the kernel heap. Ownership passes to the event loop.
 */
void notifications_handle_notification_acted_upon(Uuid *notification_id);

/**
 * @brief Announce that a notification was removed.
 *
 * Only posts the event; remove the notification from storage with notification_storage_remove().
 *
 * @param notification_id Id of the removed notification. It is copied.
 */
void notifications_handle_notification_removed(Uuid *notification_id);

/**
 * @brief Shift the timestamps of all stored notifications after a timezone change.
 *
 * @param new_tz_offset Offset in seconds subtracted from each stored timestamp.
 */
void notifications_migrate_timezone(const int new_tz_offset);

/**
 * @brief Store a notification and announce it to the system.
 *
 * The item is serialized into storage, so the caller keeps ownership of @p notification.
 *
 * @param notification Notification to add.
 */
void notifications_add_notification(TimelineItem *notification);

/** @} */

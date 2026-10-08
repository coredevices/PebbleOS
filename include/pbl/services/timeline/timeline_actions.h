/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/notifications/notification_types.h>
#include <pbl/services/timeline/item.h>

#include <applib/ui/action_menu_window_private.h>

/**
 * @defgroup services_timeline_timeline_actions Timeline action menus
 * @ingroup services_timeline
 * @brief Action menus for timeline items and the handling of action results.
 *
 * Invoking an action shows progress, waits for the PebbleSysNotificationActionResult and shows
 * the result, follows chained actions or starts a reply as the result requests.
 * @{
 */

/** @brief Where an action was invoked from. */
typedef enum TimelineItemActionSource {
  /** Notification popup. */
  TimelineItemActionSourceModalNotification,
  /** Notifications app. */
  TimelineItemActionSourceNotificationApp,
  /** Timeline app. */
  TimelineItemActionSourceTimeline,
  /** Send Text app. */
  TimelineItemActionSourceSendTextApp,
  /** Phone call UI. */
  TimelineItemActionSourcePhoneUi,
} TimelineItemActionSource;

/**
 * @brief Add an action to an action menu level.
 *
 * Response and postpone actions get a submenu; the others a plain item. The label is the
 * action's title attribute.
 *
 * @param action Action to add; must outlive the menu.
 * @param root_level Level to add it to.
 */
void timeline_actions_add_action_to_root_level(TimelineItemAction *action,
                                               ActionMenuLevel *root_level);

/**
 * @brief Create the root level of a timeline action menu.
 *
 * Also records @p source and requests a responsive phone connection while the menu is open.
 *
 * @param num_actions Number of actions the level holds.
 * @param separator_index Index of the separator, see ActionMenuLevel.
 * @param source Window or app creating the menu.
 * @return Root level.
 */
ActionMenuLevel *timeline_actions_create_action_menu_root_level(uint8_t num_actions,
                                                                uint8_t separator_index,
                                                                TimelineItemActionSource source);

/**
 * @brief Create a timeline action menu and push it.
 *
 * The menu hierarchy is destroyed when the menu closes.
 *
 * @param base_config Menu configuration; its context must be the TimelineItem of the menu.
 * @param window_stack Window stack to push to.
 * @return Action menu, or NULL if out of memory.
 */
ActionMenu *timeline_actions_push_action_menu(ActionMenuConfig *base_config,
                                              WindowStack *window_stack);

/**
 * @brief Create a reply menu from a response action and push it.
 *
 * @param item Item the menu belongs to.
 * @param reply_action Response action.
 * @param bg_color Background color of the menu.
 * @param did_close_cb Called when the menu closes.
 * @param window_stack Window stack to push to.
 * @param source Window or app pushing the menu.
 * @param standalone_reply Label the voice option "Reply with Voice", for context when no menu was
 *                         shown before.
 * @return Action menu, or NULL if out of memory.
 */
ActionMenu *timeline_actions_push_response_menu(TimelineItem *item,
                                                TimelineItemAction *reply_action, GColor bg_color,
                                                ActionMenuDidCloseCb did_close_cb,
                                                WindowStack *window_stack,
                                                TimelineItemActionSource source,
                                                bool standalone_reply);

/**
 * @brief Called when an action or batch of actions completes.
 *
 * @param succeeded Whether it succeeded.
 * @param cb_data Context given with the callback.
 */
typedef void (*ActionCompleteCallback)(bool succeeded, void *cb_data);

/**
 * @brief Invoke the dismiss action of several notifications and reminders.
 *
 * Dismisses one item per KernelMain callback so the event queue drains in between; ANCS dismissals
 * after the first use bulk mode. Failures are ignored as long as one dismissal succeeds.
 *
 * @param notif_list Items to dismiss; copied.
 * @param num_notifications Number of items.
 * @param action_menu Menu the request came from, frozen until done; may be NULL.
 * @param dismiss_all_complete_callback Called when done, may be NULL.
 * @param dismiss_all_cb_data Context for @p dismiss_all_complete_callback.
 */
void timeline_actions_dismiss_all(NotificationInfo *notif_list, int num_notifications,
                                  ActionMenu *action_menu,
                                  ActionCompleteCallback dismiss_all_complete_callback,
                                  void *dismiss_all_cb_data);

/**
 * @brief Invoke an action without an action menu.
 *
 * @param action Action to perform.
 * @param pin Item the action belongs to.
 * @param cb Called when the action completes, also immediately with false for local or failed
 *           actions; may be NULL.
 * @param cb_data Context for @p cb.
 */
void timeline_actions_invoke_action(const TimelineItemAction *action, const TimelineItem *pin,
                                    ActionCompleteCallback cb, void *cb_data);

/** @} */

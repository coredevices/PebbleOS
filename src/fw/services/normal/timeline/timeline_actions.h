/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/action_menu_window_private.h"
#include "services/normal/notifications/notification_types.h"
#include "services/normal/timeline/item.h"

typedef enum TimelineItemActionSource {
  TimelineItemActionSourceModalNotification,
  TimelineItemActionSourceNotificationApp,
  TimelineItemActionSourceTimeline,
  TimelineItemActionSourceSendTextApp,
  TimelineItemActionSourcePhoneUi,
} TimelineItemActionSource;

//! Parses a TimelineItemAction and adds it to the passed level.
//! @param action A pointer to the TimelineItemAction to add
//! @param root_level A pointer to the level the action should be added to.
void timeline_actions_add_action_to_root_level(TimelineItemAction *action,
                                               ActionMenuLevel *root_level);

//! Creates the root level for a Timeline ActionMenu, needed for holding timeline actions.
//! @param num_actions The number of actions the root level will hold.
//! @param separator_index See (struct ActionMenuLevel).
//! @return A pointer to the root level of the action menu.
ActionMenuLevel *timeline_actions_create_action_menu_root_level(uint8_t num_actions,
                                                                uint8_t separator_index,
                                                                TimelineItemActionSource source);

//! Creates a Timeline ActionMenu and pushes it to the screen
//! @param config The configuration info for this new ActionMenu. The config context must be a
//! pointer to the TimelineItem associated with this menu.
//! @param window_stack Window stack to push the action menu to
ActionMenu *timeline_actions_push_action_menu(ActionMenuConfig *base_config,
                                              WindowStack *window_stack);

//! Creates a response Timeline ActionMenu from a TimelineItemAction and pushes it to screen
//! @param item A pointer to the TimelineItem associated with this menu
//! @param reply_action A pointer to the TimelineItemAction to add
//! @param bg_color BG color of the action menu
//! @param did_close_cb Action menu did_close callback
//! @param window_stack Window stack to push the action menu to
//! @param source The window/app that pushed this action menu
//! @param standalone_reply Changes the "Voice" text to "Reply with Voice" to provide better
//! context when this menu has been pushed without a previous menu
ActionMenu *timeline_actions_push_response_menu(
    TimelineItem *item, TimelineItemAction *reply_action, GColor bg_color,
    ActionMenuDidCloseCb did_close_cb, WindowStack *window_stack, TimelineItemActionSource source,
    bool standalone_reply);

typedef void (*ActionCompleteCallback)(bool succeeded, void *cb_data);

void timeline_actions_dismiss_all(NotificationInfo *notif_list, int num_notifications,
                                  ActionMenu *action_menu,
                                  ActionCompleteCallback dismiss_all_complete_callback,
                                  void *dismiss_all_cb_data);

//! Invokes a timeline action
//! @param action The action to perform
//! @param pin The pin associated with the action
void timeline_actions_invoke_action(const TimelineItemAction *action, const TimelineItem *pin,
                                    ActionCompleteCallback cb, void *cb_data);

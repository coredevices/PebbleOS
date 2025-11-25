/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/item.h"
#include "services/normal/timeline/timeline_actions.h"
#include "util/attributes.h"

void WEAK timeline_actions_add_action_to_root_level(TimelineItemAction *action,
                                                    ActionMenuLevel *root_level) {}

ActionMenuLevel *WEAK timeline_actions_create_action_menu_root_level(
    uint8_t num_actions, uint8_t separator_index, TimelineItemActionSource source) {
  return NULL;
}

ActionMenu *timeline_actions_push_action_menu(ActionMenuConfig *base_config,
                                              WindowStack *window_stack) {
  return NULL;
}

ActionMenu *WEAK timeline_actions_push_response_menu(
    TimelineItem *item, TimelineItemAction *reply_action, GColor bg_color,
    ActionMenuDidCloseCb did_close_cb, WindowStack *window_stack, TimelineItemActionSource source,
    bool standalone_reply) {
  return NULL;
};

void WEAK timeline_actions_cleanup_action_menu(ActionMenu *action_menu, const ActionMenuItem *item,
                                               void *context) {}

void WEAK timeline_actions_dismiss_all(
    NotificationInfo *notif_list, int num_notifications, ActionMenu *action_menu,
    ActionCompleteCallback dismiss_all_complete_callback, void *dismiss_all_cb_data) {}

void WEAK timeline_actions_invoke_action(const TimelineItemAction *action, const TimelineItem *pin,
                                         ActionCompleteCallback cb, void *cb_data) {}

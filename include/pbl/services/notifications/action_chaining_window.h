/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window_stack.h"
#include "pbl/services/timeline/item.h"

/**
 * @defgroup services_notifications_action_chaining_window Action chaining window
 * @ingroup services_notifications
 * @brief Menu window to pick a follow-up action of a chained action result.
 * @{
 */

/**
 * @brief Called when the user selects an action.
 *
 * @param chaining_window The chaining window.
 * @param action Selected action.
 * @param context Context given to action_chaining_window_push().
 */
typedef void (*ActionChainingMenuSelectCb)(Window *chaining_window, TimelineItemAction *action,
                                           void *context);

/**
 * @brief Called when the chaining window is unloaded.
 *
 * @param context Context given to action_chaining_window_push().
 */
typedef void (*ActionChainingMenuClosedCb)(void *context);

/**
 * @brief Push a menu listing the actions of an action group.
 *
 * Each row shows the title and subtitle attributes of an action. @p title and @p action_group are
 * referenced, not copied, and must outlive the window.
 *
 * @param window_stack Window stack to push to.
 * @param title Title shown in the status bar.
 * @param action_group Actions to choose from.
 * @param select_cb Called when an action is selected, may be NULL.
 * @param select_cb_context Context for @p select_cb.
 * @param closed_cb Called when the window is unloaded, may be NULL.
 * @param closed_cb_context Context for @p closed_cb.
 */
void action_chaining_window_push(WindowStack *window_stack, const char *title,
                                 TimelineItemActionGroup *action_group,
                                 ActionChainingMenuSelectCb select_cb, void *select_cb_context,
                                 ActionChainingMenuClosedCb closed_cb, void *closed_cb_context);

/** @} */

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "action_menu_window.h"

//! @file action_menu_hierarchy.h
//! @addtogroup UI
//! @{
//!   @addtogroup ActionMenu
//!
//!   @{

//! Callback executed when a given action is selected
//! @param action_menu the action menu currently on screen
//! @param action the action that was triggered
//! @param context the context passed to the action menu
//! @note the action menu is closed immediately after an action is performed,
//! unless it is frozen in the ActionMenuPerformActionCb
typedef void (*ActionMenuPerformActionCb)(ActionMenu *action_menu,
                                          const ActionMenuItem *action,
                                          void *context);

//! Callback invoked for each item in an action menu hierarchy.
//! @param item the current action menu item
//! @param a caller-provided context callback
typedef void (*ActionMenuEachItemCb)(const ActionMenuItem *item, void *context);

//! enum value that controls whether menu items are displayed in a grid
//! (similarly to the emoji replies) or in a single column (reminiscent of \ref MenuLayer)
typedef enum {
  ActionMenuLevelDisplayModeWide, //!< Each item gets its own row
  ActionMenuLevelDisplayModeThin, //!< Grid view: multiple items per row
} ActionMenuLevelDisplayMode;

//! Getter for the label of a given \ref ActionMenuItem
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the string label. NULL if invalid.
char *action_menu_item_get_label(const ActionMenuItem *item);

//! Getter for the action_data pointer of a given \ref ActionMenuitem.
//! @see action_menu_level_add_action
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the data. NULL if invalid.
void *action_menu_item_get_action_data(const ActionMenuItem *item);

//! Create a new action menu level with storage allocated for a given number of items
//! @param max_items the max number of items that will be displayed at that level
//! @note levels are freed alongside the whole hierarchy so no destroy API is provided.
//! @note by default, levels are using ActionMenuLevelDisplayModeWide.
//! Use \ref action_menu_level_set_display_mode to change it.
//! @see action_menu_hierarchy_destroy
ActionMenuLevel *action_menu_level_create(uint16_t max_items);

//! Set the action menu display mode
//! @param level The ActionMenuLevel whose display mode you want to change
//! @param display_mode The display mode for the action menu (3 vs. 1 item per row)
void action_menu_level_set_display_mode(ActionMenuLevel *level,
                                        ActionMenuLevelDisplayMode display_mode);

//! Add an action to an ActionLevel
//! @param level the level to add the action to
//! @param label the text to display for the action in the menu
//! @param cb the callback that will be triggered when this action is actuated
//! @param action_data data to pass to the callback for this action
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_action(ActionMenuLevel *level,
                                             const char *label,
                                             ActionMenuPerformActionCb cb,
                                             void *action_data);

//! Add a child to this ActionMenuLevel
//! @param level the parent level
//! @param child the child level
//! @param label the text to display in the action menu for this level
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_child(ActionMenuLevel *level,
                                            ActionMenuLevel *child,
                                            const char *label);

//! Destroy a hierarchy of ActionMenuLevels
//! @param root the root level in the hierarchy
//! @param each_cb a callback to call on every \ref ActionMenuItem in every level
//! @param context a context pointer to pass to each_cb on invocation
//! @note Typical implementations will cleanup memory allocated for the item label/data
//!       associated with each item in the callback
//! @note Hierarchy is traversed in post-order.
//!       In other words, all children items are freed before their parent is freed.
void action_menu_hierarchy_destroy(const ActionMenuLevel *root,
                                   ActionMenuEachItemCb each_cb,
                                   void *context);

//!   @} // end addtogroup ActionMenu
//! @} // end addtogroup UI

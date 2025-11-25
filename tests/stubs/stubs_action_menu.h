/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/action_menu_window.h"
#include "applib/ui/action_menu_hierarchy.h"

void *action_menu_get_context(ActionMenu *action_menu) {
  return NULL;
}

ActionMenuLevel *action_menu_get_root_level(ActionMenu *action_menu) {
  return NULL;
}

ActionMenu *action_menu_open(WindowStack *window_stack, ActionMenuConfig *config) {
  return NULL;
}

ActionMenu *app_action_menu_open(ActionMenuConfig *config) {
  return NULL;
}

void action_menu_freeze(ActionMenu *action_menu) {
  return;
}

void action_menu_unfreeze(ActionMenu *action_menu) {
  return;
}

bool action_menu_is_frozen(ActionMenu *action_menu) {
  return false;
}

void action_menu_set_result_window(ActionMenu *action_menu, Window *result_window) {
  return;
}

void action_menu_set_align(ActionMenuConfig *config, ActionMenuAlign align) {
  return;
}

void action_menu_close(ActionMenu *action_menu, bool animated) {
  return;
}

char *action_menu_item_get_label(const ActionMenuItem *item) {
  return NULL;
}

void *action_menu_item_get_action_data(const ActionMenuItem *item) {
  return NULL;
}

ActionMenuLevel *action_menu_level_create(uint16_t max_items) {
  return NULL;
}

void action_menu_level_set_display_mode(ActionMenuLevel *level,
                                        ActionMenuLevelDisplayMode display_mode) {
  return;
}

ActionMenuItem *action_menu_level_add_action(ActionMenuLevel *level,
                                             const char *label,
                                             ActionMenuPerformActionCb cb,
                                             void *action_data) {
  return NULL;
}

ActionMenuItem *action_menu_level_add_child(ActionMenuLevel *level,
                                            ActionMenuLevel *child,
                                            const char *label) {
  return NULL;
}

void action_menu_hierarchy_destroy(const ActionMenuLevel *root,
                                   ActionMenuEachItemCb each_cb,
                                   void *context) {
  return;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stubs_dialog.h"
#include "applib/ui/window_stack.h"
#include "applib/ui/window_stack_private.h"


typedef struct ExpandableDialog {
  Dialog dialog;
} ExpandableDialog;

ExpandableDialog *expandable_dialog_create(const char *dialog_name) {
  return NULL;
}

ExpandableDialog *expandable_dialog_create_with_params(const char *dialog_name, uint32_t icon,
                                                       const char *text, GColor text_color,
                                                       GColor background_color,
                                                       DialogCallbacks *callbacks,
                                                       uint32_t select_icon,
                                                       ClickHandler select_click_handler) {
  return NULL;
}
Dialog *expandable_dialog_get_dialog(ExpandableDialog *expandable_dialog) {
  if (expandable_dialog == NULL) {
    return NULL;
  }
  return &expandable_dialog->dialog;
}

void expandable_dialog_push(ExpandableDialog *expandable_dialog, WindowStack *window_stack) {
  return;
}

void app_expandable_dialog_push(ExpandableDialog *expandable_dialog) {
}

void expandable_dialog_pop(ExpandableDialog *expandable_dialog) {
  return;
}

void expandable_dialog_set_select_action(ExpandableDialog *expandable_dialog,
                                         uint32_t resource_id,
                                         ClickHandler select_click_handler) {
  return;
}

void expandable_dialog_close_cb(ClickRecognizerRef recognizer, void *e_dialog) {
  return;
}

void expandable_dialog_show_action_bar(ExpandableDialog *expandable_dialog,
                                       bool show_action_bar) {
  return;
}

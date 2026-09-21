/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/confirmation_dialog.h"

static void *s_confirmation_dialog_user_data;

ConfirmationDialog *confirmation_dialog_create(const char *dialog_name) {
  return (ConfirmationDialog *)1;
}

Dialog *confirmation_dialog_get_dialog(ConfirmationDialog *confirmation_dialog) {
  return (Dialog *)1;
}

void confirmation_dialog_set_click_config_provider(ConfirmationDialog *confirmation_dialog,
                                                   ClickConfigProvider click_config_provider) {
}

void app_confirmation_dialog_push(ConfirmationDialog *confirmation_dialog) {
}

void confirmation_dialog_pop(ConfirmationDialog *confirmation_dialog) {
}

void actionable_dialog_set_user_data(ActionableDialog *actionable_dialog, void *context) {
  s_confirmation_dialog_user_data = context;
}

void *actionable_dialog_get_user_data(ActionableDialog *actionable_dialog) {
  return s_confirmation_dialog_user_data;
}

void dialog_set_text(Dialog *dialog, const char *text) {
}

void dialog_set_text_color(Dialog *dialog, GColor text_color) {
}

void dialog_set_background_color(Dialog *dialog, GColor background_color) {
}

void dialog_set_icon(Dialog *dialog, uint32_t icon_id) {
}

ActionBarLayer *confirmation_dialog_get_action_bar(ConfirmationDialog *confirmation_dialog) {
  return NULL;
}

void confirmation_dialog_push(ConfirmationDialog *confirmation_dialog, WindowStack *window_stack) {
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/dialogs/confirmation_dialog.h"

ConfirmationDialog *confirmation_dialog_create(const char *dialog_name) {
  return NULL;
}

Dialog *confirmation_dialog_get_dialog(ConfirmationDialog *confirmation_dialog) {
  return NULL;
}

ActionBarLayer *confirmation_dialog_get_action_bar(ConfirmationDialog *confirmation_dialog) {
  return NULL;
}

void confirmation_dialog_set_click_config_provider(ConfirmationDialog *confirmation_dialog,
                                                   ClickConfigProvider click_config_provider) {}

void confirmation_dialog_push(ConfirmationDialog *confirmation_dialog, WindowStack *window_stack) {}

void app_confirmation_dialog_push(ConfirmationDialog *confirmation_dialog) {}

void confirmation_dialog_pop(ConfirmationDialog *confirmation_dialog) {}

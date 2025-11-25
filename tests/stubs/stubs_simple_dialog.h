/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stubs_dialog.h"

typedef struct SimpleDialog {
  Dialog dialog;
} SimpleDialog;

SimpleDialog *simple_dialog_create(const char *dialog_name) {
  return NULL;
}

void simple_dialog_init(SimpleDialog *simple_dialog, const char *dialog_name) {
  return;
}

Dialog *simple_dialog_get_dialog(SimpleDialog *simple_dialog) {
  if (simple_dialog == NULL) {
    return NULL;
  }
  return &simple_dialog->dialog;
}

void simple_dialog_push(SimpleDialog *simple_dialog, WindowStack *window_stack) {
  return;
}

void app_simple_dialog_push(SimpleDialog *simple_dialog) {
}

bool simple_dialog_does_text_fit(const char *text, GSize window_size,
                                 GSize icon_size, bool has_status_bar) {
  return true;
}

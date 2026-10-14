/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/dialogs/simple_dialog.h>

PBL_WEAK SimpleDialog *simple_dialog_create(const char *dialog_name) {
  return nullptr;
}

PBL_WEAK void simple_dialog_init(SimpleDialog *simple_dialog, const char *dialog_name) {
  return;
}

PBL_WEAK Dialog *simple_dialog_get_dialog(SimpleDialog *simple_dialog) {
  if (simple_dialog == nullptr) {
    return nullptr;
  }
  return &simple_dialog->dialog;
}

PBL_WEAK void simple_dialog_push(SimpleDialog *simple_dialog, WindowStack *window_stack) {
  return;
}

PBL_WEAK void app_simple_dialog_push(SimpleDialog *simple_dialog) {
}

PBL_WEAK bool simple_dialog_does_text_fit(const char *text, GSize window_size, GSize icon_size,
                                          bool has_status_bar) {
  return true;
}

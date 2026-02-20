/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/action_bar_layer.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/window_stack.h"

typedef struct WorkoutDialog {
  Dialog dialog;
  ActionBarLayer action_bar;
  GBitmap confirm_icon;
  GBitmap decline_icon;
  TextLayer subtext_layer;
  char *subtext_buffer;
  bool hide_action_bar;
} WorkoutDialog;

void workout_dialog_init(WorkoutDialog *workout_dialog, const char *dialog_name);

WorkoutDialog *workout_dialog_create(const char *dialog_name);

Dialog *workout_dialog_get_dialog(WorkoutDialog *workout_dialog);

ActionBarLayer *workout_dialog_get_action_bar(WorkoutDialog *workout_dialog);

void workout_dialog_set_click_config_provider(WorkoutDialog *workout_dialog,
                                              ClickConfigProvider click_config_provider);

void workout_dialog_set_click_config_context(WorkoutDialog *workout_dialog, void *context);

void workout_dialog_push(WorkoutDialog *workout_dialog, WindowStack *window_stack);

void app_workout_dialog_push(WorkoutDialog *workout_dialog);

void workout_dialog_pop(WorkoutDialog *workout_dialog);

void workout_dialog_set_text(WorkoutDialog *workout_dialog, const char *text);

void workout_dialog_set_subtext(WorkoutDialog *workout_dialog, const char *text);

void workout_dialog_set_action_bar_hidden(WorkoutDialog *workout_dialog, bool should_hide);

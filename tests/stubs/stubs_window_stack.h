/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window_stack.h"

void window_stack_push(WindowStack *window_stack, Window *window, bool animated) { }

bool window_stack_remove(Window *window, bool animated) {
  return false;
}

void window_stack_pop_all(WindowStack *window_stack, bool animated) { }

void window_stack_insert_next(WindowStack *window_stack, Window *window) { }

bool window_stack_is_animating_with_fixed_status_bar(WindowStack *window_stack) {
  return false;
}

bool window_stack_contains_window(WindowStack *window_stack, Window *window) {
  return false;
}

bool window_transition_context_has_legacy_window_to(WindowStack *stack, Window *window){
  return false;
}

bool app_window_stack_remove(Window *window, bool animated) {
  return false;
}

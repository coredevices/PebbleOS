/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window.h"
#include "util/attributes.h"

Window * WEAK app_window_stack_pop(bool animated) {
  return NULL;
}

void WEAK app_window_stack_pop_all(const bool animated) {}

void WEAK app_window_stack_push(Window *window, bool animated) {}

Window * WEAK app_window_stack_get_top_window(void) {
  return NULL;
}

bool WEAK app_window_stack_contains_window(Window *window) {
  return false;
}

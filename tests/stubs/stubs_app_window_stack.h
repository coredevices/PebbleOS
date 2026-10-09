/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <applib/ui/window.h>

Window *PBL_WEAK app_window_stack_pop(bool animated) {
  return nullptr;
}

void PBL_WEAK app_window_stack_pop_all(const bool animated) {
}

void PBL_WEAK app_window_stack_push(Window *window, bool animated) {
}

Window *PBL_WEAK app_window_stack_get_top_window(void) {
  return nullptr;
}

bool PBL_WEAK app_window_stack_contains_window(Window *window) {
  return false;
}

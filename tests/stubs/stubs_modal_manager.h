/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window_stack_private.h"
#include "kernel/ui/modals/modal_manager.h"
#include "util/attributes.h"

WindowStack *WEAK modal_manager_get_window_stack(ModalPriority priority) {
  return NULL;
}

Window *WEAK modal_manager_get_top_window(void) {
  return NULL;
}

ClickManager *WEAK modal_manager_get_click_manager(void) {
  return NULL;
}

void WEAK modal_manager_pop_all(void) {
  return;
}

bool WEAK modal_manager_get_enabled(void) {
  return true;
}

void WEAK modal_manager_set_enabled(bool enabled) {
  return;
}

ModalProperty WEAK modal_manager_get_properties(void) {
  return ModalPropertyDefault;
}

void modal_window_push(Window *window, ModalPriority priority, bool animated) { }

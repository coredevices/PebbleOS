/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window_stack_private.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/kernel/compiler.h"

WindowStack *PBL_WEAK modal_manager_get_window_stack(ModalPriority priority) {
  return NULL;
}

Window *PBL_WEAK modal_manager_get_top_window(void) {
  return NULL;
}

ClickManager *PBL_WEAK modal_manager_get_click_manager(void) {
  return NULL;
}

void PBL_WEAK modal_manager_pop_all(void) {
  return;
}

bool PBL_WEAK modal_manager_get_enabled(void) {
  return true;
}

void PBL_WEAK modal_manager_set_enabled(bool enabled) {
  return;
}

ModalProperty PBL_WEAK modal_manager_get_properties(void) {
  return ModalPropertyDefault;
}

void modal_window_push(Window *window, ModalPriority priority, bool animated) {
}

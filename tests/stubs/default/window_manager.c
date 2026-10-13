/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/click_internal.h>
#include <applib/ui/window.h>
#include <applib/ui/window_stack_private.h>

Window *PBL_WEAK window_manager_get_top_window(void) {
  return nullptr;
}

WindowStack *PBL_WEAK window_manager_get_window_stack(void) {
  return nullptr;
}

ClickManager *PBL_WEAK window_manager_get_window_click_manager(void) {
  return nullptr;
}

PBL_WEAK bool window_manager_is_window_visible(Window *window) {
  return true;
}

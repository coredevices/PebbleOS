/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/click_internal.h"
#include "applib/ui/window.h"
#include "applib/ui/window_stack_private.h"
#include "pbl/kernel/compiler.h"

Window *PBL_WEAK window_manager_get_top_window(void) {
  return NULL;
}

WindowStack *PBL_WEAK window_manager_get_window_stack(void) {
  return NULL;
}

ClickManager *PBL_WEAK window_manager_get_window_click_manager(void) {
  return NULL;
}

bool window_manager_is_window_visible(Window *window) {
  return true;
}

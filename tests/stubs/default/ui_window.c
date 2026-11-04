/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/window_private.h>

PBL_WEAK void window_schedule_render(Window *window) {
}

PBL_WEAK GColor window_get_drawn_background_color(const Window *window) {
  return window->background_color;
}

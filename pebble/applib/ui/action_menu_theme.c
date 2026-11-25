/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_menu_theme.h"

#include <applib/dark_mode_service.h>
#include <shell/system_theme.h>

bool action_menu_theme__is_dark_mode(void) {
  // system_theme_is_dark_mode() only answers for system UI, so third-party apps query the
  // global setting through the dark mode service instead.
  return system_theme_is_system_ui() ? system_theme_is_dark_mode() : dark_mode_service_peek();
}

GColor action_menu_theme__background_color(void) {
  return action_menu_theme__is_dark_mode() ? GColorBlack : GColorWhite;
}

GColor action_menu_theme__foreground_color(void) {
  return action_menu_theme__is_dark_mode() ? GColorWhite : GColorBlack;
}

GColor action_menu_theme__highlight_background_color(void) {
  if (system_theme_is_system_ui()) {
    return system_theme_get_highlight_color();
  }
  // Third-party apps keep their own palette, so invert the theme for the highlight.
  return action_menu_theme__is_dark_mode() ? GColorWhite : GColorBlack;
}

GColor action_menu_theme__highlight_foreground_color(void) {
  return gcolor_legible_over(action_menu_theme__highlight_background_color());
}

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
  // Keep the action menu's muted, unselected text on color displays. On black and white
  // displays the theme foreground is already the only readable option.
  return PBL_IF_COLOR_ELSE(action_menu_theme__is_dark_mode() ? GColorLightGray : GColorDarkGray,
                           action_menu_theme__is_dark_mode() ? GColorWhite : GColorBlack);
}

GColor action_menu_theme__highlight_background_color(void) {
  // The focused item is distinguished by its bold text and foreground color. Invert the
  // theme on black and white displays so the selection stays visible.
  return PBL_IF_COLOR_ELSE(action_menu_theme__background_color(),
                           action_menu_theme__is_dark_mode() ? GColorWhite : GColorBlack);
}

GColor action_menu_theme__highlight_foreground_color(void) {
  return gcolor_legible_over(action_menu_theme__highlight_background_color());
}

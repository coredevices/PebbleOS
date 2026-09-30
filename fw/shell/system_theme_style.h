/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system_theme.h"

#include "applib/ui/option_menu_window.h"

//! Sizes of the system's menu cells
typedef struct SystemThemeMenuCellStyle {
  int16_t basic_cell_height;
  //! Basic cell height for third-party apps, which keep the platform's default size
  int16_t app_basic_cell_height;
  int16_t small_cell_height;
  int16_t horizontal_inset;
  int16_t title_subtitle_left_margin;
} SystemThemeMenuCellStyle;

//! Sizes of option menus
typedef struct SystemThemeOptionMenuStyle {
#if PBL_RECT
  //! Cell height for each content type, or 0 for the basic menu cell height
  uint16_t cell_heights[OptionMenuContentTypeCount];
#endif
  int16_t top_inset;
  int16_t right_icon_spacing;
  int16_t text_inset_single;
  int16_t text_inset_multi;
  int16_t right_text_inset_with_icon;
} SystemThemeOptionMenuStyle;

//! Fonts and cell geometry of the launcher for one content size
typedef struct SystemThemeLauncherStyle {
  const char *title_font_key;
  const char *subtitle_font_key;
  //! Vertical margin between the title and the subtitle
  int16_t title_margin_h;
  //! Space between a glance and the display edge, or the circle's edge on large round displays
  int16_t glance_left_inset;
  int16_t glance_right_inset;
#if PBL_RECT
  int16_t cell_height;
#else
  int16_t focused_cell_height;
  int16_t unfocused_cell_height;
#endif
} SystemThemeLauncherStyle;

//! Sizes of action menus
typedef struct SystemThemeActionMenuStyle {
  //! Content size whose Header font draws the items around the selected one on round displays
  PreferredContentSize unfocused_item_size;
} SystemThemeActionMenuStyle;

//! Everything the system UI draws differently for each preferred content size
typedef struct SystemThemeStyle {
  //! Font keys for each text style
  const char *fonts[TextStyleFontCount];
  SystemThemeMenuCellStyle menu_cell;
  SystemThemeOptionMenuStyle option_menu;
  SystemThemeActionMenuStyle action_menu;
#if !defined(CONFIG_RECOVERY_FW)
  SystemThemeLauncherStyle launcher;
#endif
} SystemThemeStyle;

//! @return The style for the user's preferred content size
const SystemThemeStyle *system_theme_get_style(void);

//! @return The style for the given content size
const SystemThemeStyle *system_theme_get_style_for_size(PreferredContentSize size);

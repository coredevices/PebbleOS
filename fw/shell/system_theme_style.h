/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system_theme.h"

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
  //! Cell heights for single and double line content, or 0 for the basic menu cell height
  int16_t single_line_cell_height;
  int16_t double_line_cell_height;
#endif
  int16_t top_inset;
  int16_t right_icon_spacing;
  int16_t text_inset_single;
  int16_t text_inset_multi;
  int16_t right_text_inset_with_icon;
} SystemThemeOptionMenuStyle;

//! Smallest launcher cell heights of any content size, bounding how many rows can be visible
#if PBL_RECT
#define SYSTEM_THEME_LAUNCHER_MIN_CELL_HEIGHT (42)
#else
#define SYSTEM_THEME_LAUNCHER_MIN_FOCUSED_CELL_HEIGHT   (52)
#define SYSTEM_THEME_LAUNCHER_MIN_UNFOCUSED_CELL_HEIGHT (38)
#endif

//! Fonts and cell geometry of the launcher for one content size
typedef struct SystemThemeLauncherStyle {
  const char *title_font_key;
  const char *subtitle_font_key;
  //! Vertical margin between the title and the subtitle
  int16_t title_margin_h;
  //! Space between a glance and the display edge, or the circle's edge on large round displays
  int16_t glance_inset;
#if PBL_RECT
  int16_t cell_height;
#else
  int16_t focused_cell_height;
  int16_t unfocused_cell_height;
#endif
} SystemThemeLauncherStyle;

//! Everything the system UI draws differently for each preferred content size
typedef struct SystemThemeStyle {
  //! Font keys for each text style
  const char *fonts[TextStyleFontCount];
  SystemThemeMenuCellStyle menu_cell;
  SystemThemeOptionMenuStyle option_menu;
#if !defined(CONFIG_RECOVERY_FW)
  SystemThemeLauncherStyle launcher;
#endif
} SystemThemeStyle;

//! @return The style for the user's preferred content size
const SystemThemeStyle *system_theme_get_style(void);

//! @return The style for the given content size
const SystemThemeStyle *system_theme_get_style_for_size(PreferredContentSize size);

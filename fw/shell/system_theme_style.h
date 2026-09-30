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

//! Everything the system UI draws differently for each preferred content size
typedef struct SystemThemeStyle {
  //! Font keys for each text style
  const char *fonts[TextStyleFontCount];
  SystemThemeMenuCellStyle menu_cell;
} SystemThemeStyle;

//! @return The style for the user's preferred content size
const SystemThemeStyle *system_theme_get_style(void);

//! @return The style for the given content size
const SystemThemeStyle *system_theme_get_style_for_size(PreferredContentSize size);

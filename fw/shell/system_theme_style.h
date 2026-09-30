/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system_theme.h"

//! Everything the system UI draws differently for each preferred content size
typedef struct SystemThemeStyle {
  //! Font keys for each text style
  const char *fonts[TextStyleFontCount];
} SystemThemeStyle;

//! @return The style for the user's preferred content size
const SystemThemeStyle *system_theme_get_style(void);

//! @return The style for the given content size
const SystemThemeStyle *system_theme_get_style_for_size(PreferredContentSize size);

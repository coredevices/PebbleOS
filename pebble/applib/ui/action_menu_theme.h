/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/graphics/graphics.h>

//! @return true if ActionMenu should render in dark mode.
bool action_menu_theme__is_dark_mode(void);

//! @return the menu background color for the current theme.
GColor action_menu_theme__background_color(void);

//! @return the menu foreground color for the current theme.
GColor action_menu_theme__foreground_color(void);

//! @return the highlight background color for the current theme.
GColor action_menu_theme__highlight_background_color(void);

//! @return the highlight foreground color for the current theme.
GColor action_menu_theme__highlight_foreground_color(void);

/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "menu.h"

#include <stddef.h>

#include <applib/graphics/gtypes.h>
#include <applib/ui/time_range_selection_window.h>

//! Dark mode option menu and schedule window shared by the Display and Themes settings pages.
typedef struct DarkModeMenu {
  //! Settings page that is reloaded when the dark mode prefs change
  SettingsMenuItem menu_item;
  //! Highlight color of the schedule window, read each time it is pushed
  GColor (*get_highlight_color)(void);
  TimeRangeSelectionWindowData schedule_window;
} DarkModeMenu;

void dark_mode_menu_init(DarkModeMenu *menu, SettingsMenuItem menu_item,
                         GColor (*get_highlight_color)(void));

void dark_mode_menu_deinit(DarkModeMenu *menu);

//! Pushes the Off/On/Ambient/Schedule option menu. \a menu must outlive it.
void dark_mode_menu_push(DarkModeMenu *menu);

//! Pushes the schedule time range window. \a menu must outlive it.
void dark_mode_menu_schedule_push(DarkModeMenu *menu);

//! @return The i18n key for the label of the current dark mode.
const char *dark_mode_menu_get_mode_label(void);

//! Formats the current schedule as "<from> - <to>".
void dark_mode_menu_format_schedule(char *buf, size_t buf_size);

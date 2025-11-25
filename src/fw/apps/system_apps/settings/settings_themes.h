/* SPDX-FileCopyrightText: 2025 Elad Dvash */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "settings_menu.h"
#include "shell/system_theme.h"

#ifndef DEFAULT_SETTINGS_HIGHLIGHT_COLOR
#define DEFAULT_SETTINGS_HIGHLIGHT_COLOR GColorCobaltBlue
#define DEFAULT_APPS_HIGHLIGHT_COLOR GColorVividCerulean
#endif

typedef struct ColorDefinition {
  const char *name;
  const GColor light;
  const GColor dark;
} ColorDefinition;

static const ColorDefinition s_color_definitions[11] = {
  {"Default", GColorClear},
  {"Red", GColorSunsetOrange, GColorDarkCandyAppleRed},
  {"Orange", GColorChromeYellow, GColorWindsorTan},
  {"Yellow", GColorYellow, GColorArmyGreen},
  {"Green", GColorGreen, GColorDarkGreen},
  {"Cyan", GColorCyan, GColorMidnightGreen},
  {"Light Blue", GColorVividCerulean, GColorCobaltBlue},
  {"Royal Blue", GColorVeryLightBlue, GColorDukeBlue},
  {"Purple", GColorLavenderIndigo, GColorIndigo},
  {"Magenta", GColorMagenta, GColorPurple},
  {"Pink", GColorBrilliantRose, GColorJazzberryJam},
};

const SettingsModuleMetadata *settings_themes_get_info(void);

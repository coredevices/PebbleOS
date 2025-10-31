/*
 * Copyright 2025 Elad Dvash
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

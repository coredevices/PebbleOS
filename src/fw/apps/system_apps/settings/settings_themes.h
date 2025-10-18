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
  const GColor value;
} ColorDefinition;

static const ColorDefinition s_color_definitions[65] = {
  // Pebble colors taken from gcolor_definitions.h
  {"Default", GColorClear},
  {"Black", GColorBlack},
  {"Blue", GColorBlue},
  {"Green", GColorGreen},
  {"Cyan", GColorCyan},
  {"Purple", GColorPurple},
  {"Red", GColorRed},
  {"Magenta", GColorMagenta},
  {"Orange", GColorOrange},
  {"Yellow", GColorYellow},
  {"Gray", GColorDarkGray},

  {"Oxford Blue", GColorOxfordBlue},
  {"Duke Blue", GColorDukeBlue},
  {"Dark Green", GColorDarkGreen},
  {"Midnight Green", GColorMidnightGreen},
  {"Cobalt Blue", GColorCobaltBlue},
  {"Blue Moon", GColorBlueMoon},
  {"Islamic Green", GColorIslamicGreen},
  {"Jaeger Green", GColorJaegerGreen},
  {"Tiffany Blue", GColorTiffanyBlue},
  {"Vivid Cerulean", GColorVividCerulean},
  {"Malachite", GColorMalachite},
  {"Medium Spring Green", GColorMediumSpringGreen},
  {"Bulgarian Rose", GColorBulgarianRose},
  {"Imperial Purple", GColorImperialPurple},
  {"Indigo", GColorIndigo},
  {"Electric Ultramarine", GColorElectricUltramarine},
  {"Army Green", GColorArmyGreen},
  {"Liberty", GColorLiberty},
  {"Very Light Blue", GColorVeryLightBlue},
  {"Kelly Green", GColorKellyGreen},
  {"May Green", GColorMayGreen},
  {"Cadet Blue", GColorCadetBlue},
  {"Picton Blue", GColorPictonBlue},
  {"Bright Green", GColorBrightGreen},
  {"Screamin Green", GColorScreaminGreen},
  {"Medium Aquamarine", GColorMediumAquamarine},
  {"Electric Blue", GColorElectricBlue},
  {"Dark Candy Apple Red", GColorDarkCandyAppleRed},
  {"Jazzberry Jam", GColorJazzberryJam},
  {"Vivid Violet", GColorVividViolet},
  {"Windsor Tan", GColorWindsorTan},
  {"Rose Vale", GColorRoseVale},
  {"Purpureus", GColorPurpureus},
  {"Lavender Indigo", GColorLavenderIndigo},
  {"Limerick", GColorLimerick},
  {"Brass", GColorBrass},
  {"Light Gray", GColorLightGray},
  {"Baby Blue Eyes", GColorBabyBlueEyes},
  {"Spring Bud", GColorSpringBud},
  {"Inchworm", GColorInchworm},
  {"Mint Green", GColorMintGreen},
  {"Celeste", GColorCeleste},
  {"Folly", GColorFolly},
  {"Fashion Magenta", GColorFashionMagenta},
  {"Sunset Orange", GColorSunsetOrange},
  {"Brilliant Rose", GColorBrilliantRose},
  {"Shocking Pink", GColorShockingPink},
  {"Chrome Yellow", GColorChromeYellow},
  {"Rajah", GColorRajah},
  {"Melon", GColorMelon},
  {"Rich Brilliant Lavender", GColorRichBrilliantLavender},
  {"Icterine", GColorIcterine},
  {"Pastel Yellow", GColorPastelYellow},
  {"White", GColorWhite},
};

static const ColorDefinition s_color_definitions_short[12] = {
  {"Default", GColorClear},
  {"Black", GColorBlack},
  {"Blue", GColorBlue},
  {"Green", GColorGreen},
  {"Cyan", GColorCyan},
  {"Purple", GColorPurple},
  {"Red", GColorRed},
  {"Magenta", GColorMagenta},
  {"Orange", GColorOrange},
  {"Yellow", GColorYellow},
  {"Gray", GColorDarkGray},
  {"Show All...", GColorClear}, // Placeholder color for "Show All" option
};

const SettingsModuleMetadata *settings_themes_get_info(void);

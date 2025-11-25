/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once 

#include "applib/graphics/text.h"
#include <inttypes.h>

#define FONT_HEIGHT 10

uint8_t fonts_get_font_height(GFont font) {
  return FONT_HEIGHT;
}

GFont fonts_get_system_font(const char *font_key) {
  return NULL;
}

FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height) {
  return NULL;
}


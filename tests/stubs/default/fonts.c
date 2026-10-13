/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/compiler.h>

#include <applib/graphics/text.h>

#define FONT_HEIGHT 10

PBL_WEAK uint8_t fonts_get_font_height(GFont font) {
  return FONT_HEIGHT;
}

PBL_WEAK GFont fonts_get_system_font(const char *font_key) {
  return nullptr;
}

PBL_WEAK FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height) {
  return nullptr;
}

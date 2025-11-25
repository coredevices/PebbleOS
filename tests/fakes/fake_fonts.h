/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/fonts/fonts.h"

GFont fonts_get_system_font(const char *font_key);

uint8_t fonts_get_font_height(GFont font);

GFont system_resource_get_font(const char *font_key);

FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height);

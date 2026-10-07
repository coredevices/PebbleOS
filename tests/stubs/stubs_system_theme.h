/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

#include <pbl/kernel/compiler.h>

#include <shell/system_theme.h>
#include <stubs_ambient_light.h>
#include <stubs_pebble_process_md.h>

const char *PBL_WEAK system_theme_get_font_key(TextStyleFont font) {
  return NULL;
}

GColor PBL_WEAK system_theme_get_bg_color(void) {
  return GColorWhite;
}

GColor PBL_WEAK system_theme_get_fg_color(void) {
  return GColorBlack;
}

bool PBL_WEAK system_theme_is_system_ui(void) {
  return true;
}

bool PBL_WEAK system_theme_is_dark_mode(void) {
  return false;
}

void PBL_WEAK system_theme_refresh_ambient(void) {
}

const char *PBL_WEAK system_theme_get_font_key_for_size(PreferredContentSize size,
                                                        TextStyleFont font) {
  return NULL;
}

GFont PBL_WEAK system_theme_get_font(TextStyleFont font) {
  return NULL;
}

GFont PBL_WEAK system_theme_get_font_for_default_size(TextStyleFont font) {
  return NULL;
}

PreferredContentSize PBL_WEAK system_theme_get_default_content_size_for_runtime_platform(void) {
  return PreferredContentSizeDefault;
}

PreferredContentSize PBL_WEAK
system_theme_convert_host_content_size_to_runtime_platform(PreferredContentSize size) {
  return size;
}

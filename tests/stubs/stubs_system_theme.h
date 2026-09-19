/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/system_theme.h"
#include "pbl/kernel/compiler.h"

#include <stdlib.h>

const char *PBL_WEAK system_theme_get_font_key(TextStyleFont font) {
  return NULL;
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

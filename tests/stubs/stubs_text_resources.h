/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once 

#include "applib/fonts/fonts_private.h"
#include "applib/graphics/text_resources.h"
#include "applib/fonts/codepoint.h"

#include <inttypes.h>
#include <stdbool.h>

#define HORIZ_ADVANCE_PX (2)

bool text_resources_setup_font(FontCache* font_cache, FontInfo* fontinfo) {
  return true;
}

int8_t text_resources_get_glyph_horiz_advance(FontCache* font_cache, Codepoint codepoint, FontInfo* fontinfo) {
  if (codepoint_is_zero_width(codepoint)) {
    return 0;
  }
  // Real fonts have some weird values here, give something totally bogus for testing.
  if (codepoint == '\n') {
    return 5;
  }
  return HORIZ_ADVANCE_PX;
}

int8_t text_resources_get_glyph_height(FontCache* font_cache, Codepoint codepoint, FontInfo* fontinfo) {
  return 10;
}


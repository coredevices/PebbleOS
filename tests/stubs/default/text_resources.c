/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/compiler.h>

#include <applib/fonts/codepoint.h>
#include <applib/fonts/fonts_private.h>
#include <applib/graphics/text_resources.h>
#include <stubs_text_resources.h>

PBL_WEAK bool text_resources_setup_font(FontCache *font_cache, FontInfo *fontinfo) {
  return true;
}

PBL_WEAK int8_t text_resources_get_glyph_horiz_advance(FontCache *font_cache, Codepoint codepoint,
                                                       FontInfo *fontinfo) {
  if (codepoint_is_zero_width(codepoint)) {
    return 0;
  }
  // Real fonts have some weird values here, give something totally bogus for testing.
  if (codepoint == '\n') {
    return 5;
  }
  return HORIZ_ADVANCE_PX;
}

PBL_WEAK int8_t text_resources_get_glyph_height(FontCache *font_cache, Codepoint codepoint,
                                                FontInfo *fontinfo) {
  return 10;
}

PBL_WEAK const GlyphData *text_resources_get_glyph(FontCache *font_cache, Codepoint codepoint,
                                                   FontInfo *fontinfo,
                                                   GlyphLocation *location_out) {
  if (location_out) {
    *location_out = (GlyphLocation){};
  }
  return nullptr;
}

PBL_WEAK bool text_resources_glyph_is_color(const GlyphLocation *location) {
  return false;
}

PBL_WEAK void text_resources_reset_cache(FontCache *font_cache) {
}

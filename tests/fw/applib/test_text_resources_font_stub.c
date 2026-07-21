/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Strong override of the weak sys_font_get_system_font() stub for the text_resources test. The
// renderer obtains the per-glyph fallback font from fonts_get_fallback_font(), which calls
// sys_font_get_system_font(NULL); a test installs a fallback by pointing s_test_fallback_font at
// its own FontInfo. This lives in a separate translation unit so it overrides the weak stub at
// link time rather than colliding with it.

#include "applib/fonts/fonts.h"

FontInfo *s_test_fallback_font;

// The only named keys the code under test asks for are the system emoji fonts, requested through
// fonts_get_system_emoji_font_for_size(); a test installs one by pointing s_test_emoji_font at its
// own FontInfo.
FontInfo *s_test_emoji_font;

GFont sys_font_get_system_font(const char *key) {
  if (key == NULL) {
    return s_test_fallback_font;
  }
  return s_test_emoji_font;
}

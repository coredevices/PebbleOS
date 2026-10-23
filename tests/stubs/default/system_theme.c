/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

const char *PBL_WEAK system_theme_get_font_key(TextStyleFont font) {
  return nullptr;
}

const char *PBL_WEAK system_theme_get_font_key_for_size(PreferredContentSize size,
                                                        TextStyleFont font) {
  return nullptr;
}

GFont PBL_WEAK system_theme_get_font(TextStyleFont font) {
  return nullptr;
}

GFont PBL_WEAK system_theme_get_font_for_size(PreferredContentSize size, TextStyleFont font) {
  return nullptr;
}

GFont PBL_WEAK system_theme_get_font_for_default_size(TextStyleFont font) {
  return nullptr;
}

PreferredContentSize PBL_WEAK system_theme_get_default_content_size_for_runtime_platform(void) {
  return PreferredContentSizeDefault;
}

PreferredContentSize PBL_WEAK
system_theme_convert_host_content_size_to_runtime_platform(PreferredContentSize size) {
  return size;
}

void PBL_WEAK system_theme_get_colors(SystemThemeColors *colors_out) {
  if (!colors_out) {
    return;
  }
  const GColor primary = shell_prefs_get_theme_highlight_color();
  const GColor secondary = shell_prefs_get_theme_secondary_color();
  const GColor bg = shell_prefs_get_theme_background_color();
  GColor text = shell_prefs_get_theme_text_color();
  if (gcolor_is_transparent(text)) {
    text = gcolor_legible_over(bg);
  }
  *colors_out = (SystemThemeColors){
    .primary_color = primary,
    .primary_text_color = gcolor_legible_over(primary),
    .secondary_color = secondary,
    .secondary_text_color = gcolor_legible_over(secondary),
    .background_color = bg,
    .text_color = text,
    .subtitle_color =
        PBL_IF_COLOR_ELSE(gcolor_equal(text, GColorWhite) ? GColorLightGray : GColorDarkGray, text),
  };
}

static bool prv_use_default_app_colors(void) {
#if !PBL_COLOR
  return true;
#else
  return (shell_prefs_get_theme_app_color_mode() == ThemeAppColorMode_BackgroundOnly) ||
         (shell_prefs_is_theme_background_default() &&
          gcolor_equal(shell_prefs_get_theme_highlight_color(), GColorVividCerulean) &&
          gcolor_equal(shell_prefs_get_theme_secondary_color(), GColorCobaltBlue));
#endif
}

void PBL_WEAK system_theme_get_app_colors(GColor default_primary, GColor default_secondary,
                                          SystemThemeColors *colors_out) {
  if (!colors_out) {
    return;
  }
  const bool use_defaults = prv_use_default_app_colors();
  const GColor primary = use_defaults ? default_primary : shell_prefs_get_theme_highlight_color();
  const GColor secondary =
      use_defaults ? default_secondary : shell_prefs_get_theme_secondary_color();
  const GColor bg = shell_prefs_get_theme_background_color();
  GColor text = shell_prefs_get_theme_text_color();
  if (gcolor_is_transparent(text)) {
    text = gcolor_legible_over(bg);
  }
  *colors_out = (SystemThemeColors){
    .primary_color = primary,
    .primary_text_color = gcolor_legible_over(primary),
    .secondary_color = secondary,
    .secondary_text_color = gcolor_legible_over(secondary),
    .background_color = bg,
    .text_color = text,
    .subtitle_color =
        PBL_IF_COLOR_ELSE(gcolor_equal(text, GColorWhite) ? GColorLightGray : GColorDarkGray, text),
  };
}

GColor PBL_WEAK system_theme_get_primary_color(void) {
  return shell_prefs_get_theme_highlight_color();
}

GColor PBL_WEAK system_theme_get_secondary_color(void) {
  return shell_prefs_get_theme_secondary_color();
}

GColor PBL_WEAK system_theme_get_background_color(void) {
  return shell_prefs_get_theme_background_color();
}

GColor PBL_WEAK system_theme_get_text_color(void) {
  GColor text = shell_prefs_get_theme_text_color();
  return gcolor_is_transparent(text) ? gcolor_legible_over(shell_prefs_get_theme_background_color())
                                     : text;
}

GBitmap *PBL_WEAK system_theme_create_background_bitmap(void) {
  return nullptr;
}

void PBL_WEAK system_theme_draw_background(GContext *ctx, const GRect *bounds,
                                           GColor fallback_bg_color,
                                           const GBitmap *cached_bg_bitmap) {
  if (!ctx || !bounds) {
    return;
  }
  if (!gcolor_is_transparent(fallback_bg_color)) {
    graphics_context_set_fill_color(ctx, fallback_bg_color);
    graphics_fill_rect(ctx, bounds);
  }
  if (cached_bg_bitmap) {
    graphics_draw_bitmap_in_rect(ctx, cached_bg_bitmap, bounds);
  }
}

void PBL_WEAK system_theme_reset_to_default(bool dark_mode) {
}

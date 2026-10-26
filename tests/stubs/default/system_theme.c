/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics.h>
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
  if (colors_out) {
    *colors_out = (SystemThemeColors){
      .primary_color = GColorVividCerulean,
      .primary_text_color = GColorWhite,
      .secondary_color = GColorCobaltBlue,
      .secondary_text_color = GColorWhite,
      .background_color = GColorWhite,
      .text_color = GColorBlack,
      .subtitle_color = GColorDarkGray,
    };
  }
}

void PBL_WEAK system_theme_get_app_colors(GColor default_primary, GColor default_secondary,
                                          SystemThemeColors *colors_out) {
  if (colors_out) {
    *colors_out = (SystemThemeColors){
      .primary_color = default_primary,
      .primary_text_color = GColorWhite,
      .secondary_color = default_secondary,
      .secondary_text_color = GColorWhite,
      .background_color = GColorWhite,
      .text_color = GColorBlack,
      .subtitle_color = GColorDarkGray,
    };
  }
}

GColor PBL_WEAK system_theme_get_primary_color(void) {
  return GColorVividCerulean;
}

GColor PBL_WEAK system_theme_get_secondary_color(void) {
  return GColorCobaltBlue;
}

GColor PBL_WEAK system_theme_get_background_color(void) {
  return GColorWhite;
}

GColor PBL_WEAK system_theme_get_text_color(void) {
  return GColorBlack;
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

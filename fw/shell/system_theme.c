/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system_theme.h"

#include "system_theme_style.h"

#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/gtypes.h>
#include <process_management/process_manager.h>
#include <resource/resource.h>
#include <shell/prefs.h>

////////////////////
// Helpers

static const char *prv_get_font_for_size(bool from_user, PreferredContentSize content_size,
                                         TextStyleFont font) {
  if (content_size >= NumPreferredContentSizes) {
    PBL_LOG_ERR("Requested a content size that is out of bounds (%d)", content_size);
    goto fail;
  } else if (font >= TextStyleFontCount) {
    PBL_LOG_ERR("Requested a style font that is out of bounds (%d)", font);
    goto fail;
  }
  return system_theme_get_style_for_size(content_size)->fonts[font];
fail:
  from_user ? pbl_syscall_fail() : pbl_panic();
}

////////////////////
// Public API

// *** WARNING WARNING WARNING ***
// Be very careful when modifying this syscall. It currently returns a pointer
// to constant data in flash, which unprivileged apps are allowed to read. But
// if the data pointed to is ever moved to RAM, the syscall will need to be
// changed to copy the data into a caller-provided buffer. Unprivileged apps
// are not allowed to read kernel RAM, so they will fault if they attempt to
// dereference a pointer into kernel RAM.
PBL_SYSCALL(const char *, system_theme_get_font_key, (VAL, TextStyleFont, font)) {
  return prv_get_font_for_size(from_user, system_theme_get_content_size(), font);
}

// *** WARNING WARNING WARNING ***
// Be very careful when modifying this syscall. It currently returns a pointer
// to constant data in flash, which unprivileged apps are allowed to read.
PBL_SYSCALL(const char *, system_theme_get_font_key_for_size,
            (VAL, PreferredContentSize, content_size), (VAL, TextStyleFont, font)) {
  const PreferredContentSize size_on_runtime_platform =
      system_theme_convert_host_content_size_to_runtime_platform(content_size);
  return prv_get_font_for_size(from_user, size_on_runtime_platform, font);
}

GFont system_theme_get_font(TextStyleFont font) {
  return fonts_get_system_font(system_theme_get_font_key(font));
}

GFont system_theme_get_font_for_size(PreferredContentSize size, TextStyleFont font) {
  return fonts_get_system_font(system_theme_get_font_key_for_size(size, font));
}

GFont system_theme_get_font_for_default_size(TextStyleFont font) {
  return fonts_get_system_font(
      system_theme_get_font_key_for_size(PreferredContentSizeDefault, font));
}

static const PreferredContentSize s_platform_default_content_sizes[] = {
  [PlatformTypeAplite] = PreferredContentSizeMedium,
  [PlatformTypeBasalt] = PreferredContentSizeMedium,
  [PlatformTypeChalk] = PreferredContentSizeMedium,
  [PlatformTypeDiorite] = PreferredContentSizeMedium,
  [PlatformTypeEmery] = PreferredContentSizeLarge,
  [PlatformTypeFlint] = PreferredContentSizeMedium,
  [PlatformTypeGabbro] = PreferredContentSizeLarge,
};

PBL_T_STATIC PreferredContentSize prv_convert_content_size_between_platforms(
    PreferredContentSize size, PlatformType from_platform, PlatformType to_platform) {
  const size_t num_platform_default_content_sizes = ARRAY_LENGTH(s_platform_default_content_sizes);
  PBL_ASSERTN(from_platform < num_platform_default_content_sizes);
  PBL_ASSERTN(to_platform < num_platform_default_content_sizes);

  const PreferredContentSize from_platform_default_size =
      s_platform_default_content_sizes[from_platform];
  const PreferredContentSize to_platform_default_size =
      s_platform_default_content_sizes[to_platform];
  const int resulting_size = size + (to_platform_default_size - from_platform_default_size);
  return (PreferredContentSize)CLIP(resulting_size, 0, (NumPreferredContentSizes - 1));
}

PreferredContentSize system_theme_get_default_content_size_for_runtime_platform(void) {
  const PlatformType runtime_platform = process_manager_current_platform();
  return prv_convert_content_size_between_platforms(PreferredContentSizeDefault,
                                                    PBL_PLATFORM_TYPE_CURRENT, runtime_platform);
}

PreferredContentSize system_theme_convert_host_content_size_to_runtime_platform(
    PreferredContentSize size) {
  const PlatformType runtime_platform = process_manager_current_platform();
  return prv_convert_content_size_between_platforms(size, PBL_PLATFORM_TYPE_CURRENT,
                                                    runtime_platform);
}

static void prv_resolve_colors(GColor primary, GColor secondary, GColor bg, GColor text_pref,
                               SystemThemeColors *colors_out) {
  GColor text = gcolor_is_transparent(text_pref) ? gcolor_legible_over(bg) : text_pref;
  GColor subtitle =
      PBL_IF_COLOR_ELSE(gcolor_equal(text, GColorWhite) ? GColorLightGray : GColorDarkGray, text);
  *colors_out = (SystemThemeColors){
    .primary_color = primary,
    .primary_text_color = gcolor_legible_over(primary),
    .secondary_color = secondary,
    .secondary_text_color = gcolor_legible_over(secondary),
    .background_color = bg,
    .text_color = text,
    .subtitle_color = subtitle,
  };
}

PBL_SYSCALL(void, system_theme_get_colors, (OUT, SystemThemeColors *, colors_out)) {
  prv_resolve_colors(
      shell_prefs_get_theme_highlight_color(), shell_prefs_get_theme_secondary_color(),
      shell_prefs_get_theme_background_color(), shell_prefs_get_theme_text_color(), colors_out);
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

void system_theme_get_app_colors(GColor default_primary, GColor default_secondary,
                                 SystemThemeColors *colors_out) {
  if (!colors_out) {
    return;
  }
  const bool use_defaults = prv_use_default_app_colors();
  const GColor primary = use_defaults ? default_primary : shell_prefs_get_theme_highlight_color();
  const GColor secondary =
      use_defaults ? default_secondary : shell_prefs_get_theme_secondary_color();
  prv_resolve_colors(primary, secondary, shell_prefs_get_theme_background_color(),
                     shell_prefs_get_theme_text_color(), colors_out);
}

PBL_SYSCALL(GColor, system_theme_get_primary_color) {
  return shell_prefs_get_theme_highlight_color();
}

PBL_SYSCALL(GColor, system_theme_get_secondary_color) {
  return shell_prefs_get_theme_secondary_color();
}

PBL_SYSCALL(GColor, system_theme_get_background_color) {
  return shell_prefs_get_theme_background_color();
}

PBL_SYSCALL(GColor, system_theme_get_text_color) {
  GColor text = shell_prefs_get_theme_text_color();
  if (gcolor_is_transparent(text)) {
    return gcolor_legible_over(shell_prefs_get_theme_background_color());
  }
  return text;
}

GBitmap *system_theme_create_background_bitmap(void) {
  const uint32_t bg_res_id = shell_prefs_get_theme_bg_resource_id();
  if (bg_res_id == 0) {
    return nullptr;
  }
  return gbitmap_create_with_resource_system(SYSTEM_APP, bg_res_id);
}

void system_theme_draw_background(GContext *ctx, const GRect *bounds, GColor fallback_bg_color,
                                  const GBitmap *cached_bg_bitmap) {
  if (!ctx || !bounds) {
    return;
  }
  if (!gcolor_is_transparent(fallback_bg_color)) {
    graphics_context_set_fill_color(ctx, fallback_bg_color);
    graphics_fill_rect(ctx, bounds);
  }
#if PBL_COLOR
  if (cached_bg_bitmap) {
    graphics_draw_bitmap_in_rect(ctx, cached_bg_bitmap, bounds);
  } else if (shell_prefs_get_theme_bg_resource_id() != 0) {
    GBitmap *bg = system_theme_create_background_bitmap();
    if (bg) {
      graphics_draw_bitmap_in_rect(ctx, bg, bounds);
      gbitmap_destroy(bg);
    }
  }
#endif
}

void system_theme_reset_to_default(bool dark_mode) {
  const Uuid invalid_uuid = UUID_INVALID;
  shell_prefs_set_theme_active_uuid(&invalid_uuid);
  shell_prefs_set_theme_highlight_color(
      PBL_IF_COLOR_ELSE(dark_mode ? GColorCobaltBlue : GColorVividCerulean, GColorBlack));
  shell_prefs_set_theme_secondary_color(
      PBL_IF_COLOR_ELSE(dark_mode ? GColorOxfordBlue : GColorCobaltBlue, GColorBlack));
  shell_prefs_set_theme_background_color(
      PBL_IF_COLOR_ELSE(dark_mode ? GColorBlack : GColorClear, GColorClear));
  shell_prefs_set_theme_text_color(GColorClear);
  shell_prefs_set_theme_bg_resource_id(0);
}

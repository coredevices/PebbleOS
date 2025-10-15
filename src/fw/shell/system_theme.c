/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system_theme.h"

#include "applib/fonts/fonts.h"
#include "apps/system_apps/settings/settings_notifications_private.h"
#include "process_management/process_manager.h"
#include "services/common/analytics/analytics.h"
#include "shell/prefs.h"
#include "syscall/syscall_internal.h"
#include "system/passert.h"
#include "util/size.h"

#include <string.h>

typedef struct SystemThemeTextStyle {
  const char *fonts[TextStyleFontCount];
} SystemThemeTextStyle;

////////////////////
// Themes

static const SystemThemeTextStyle s_text_styles[NumPreferredContentSizes] = {
  [PreferredContentSizeSmall] = {
    .fonts = {
      [TextStyleFont_Header] = FONT_KEY_GOTHIC_18_BOLD,
#if !RECOVERY_FW
      [TextStyleFont_Title] = FONT_KEY_GOTHIC_18_BOLD,
      [TextStyleFont_Body] = FONT_KEY_GOTHIC_18,
#endif
      [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_18_BOLD,
      [TextStyleFont_Caption] = FONT_KEY_GOTHIC_14,
      [TextStyleFont_Footer] = FONT_KEY_GOTHIC_14,
      //! @note this is the same as the Title key (as that's what it's cloned from) until Small
      //!       is designed
      [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_18_BOLD,
      //! @note this is the same as Medium until Small is designed
      [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_18,
#if !RECOVERY_FW
      //! @note this is the same as Medium until Small is designed
      [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_20_BOLD_NUMBERS,
#endif
      //! @note this is the same as Medium until Small is designed
      [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_14_BOLD,
      //! @note this is the same as Medium until Small is designed
      [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_18,
      //! @note this is the same as Medium until Small is designed
      [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_14,
    },
  },
  [PreferredContentSizeMedium] = {
    .fonts = {
      [TextStyleFont_Header] = FONT_KEY_GOTHIC_18_BOLD,
#if !RECOVERY_FW
      [TextStyleFont_Title] = FONT_KEY_GOTHIC_24_BOLD,
      [TextStyleFont_Body] = FONT_KEY_GOTHIC_24_BOLD,
#endif
      [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_24_BOLD,
      [TextStyleFont_Caption] = FONT_KEY_GOTHIC_14,
      [TextStyleFont_Footer] = FONT_KEY_GOTHIC_18,
      [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_24_BOLD,
      [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_18,
#if !RECOVERY_FW
      [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_20_BOLD_NUMBERS,
#endif
      [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_14_BOLD,
      [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_18,
      [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_14,
    },
  },
  [PreferredContentSizeLarge] = {
    .fonts = {
      [TextStyleFont_Header] = FONT_KEY_GOTHIC_24_BOLD,
#if !RECOVERY_FW
      [TextStyleFont_Title] = FONT_KEY_GOTHIC_28_BOLD,
      [TextStyleFont_Body] = FONT_KEY_GOTHIC_28,
#endif
      [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_28,
      [TextStyleFont_Caption] = FONT_KEY_GOTHIC_18,
      [TextStyleFont_Footer] = FONT_KEY_GOTHIC_18,
      [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_24_BOLD,
      [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_24,
#if !RECOVERY_FW
      [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
#endif
      [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_18_BOLD,
      [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_18_BOLD,
    },
  },
  [PreferredContentSizeExtraLarge] = {
    .fonts = {
      [TextStyleFont_Header] = FONT_KEY_GOTHIC_28_BOLD,
#if !RECOVERY_FW
      [TextStyleFont_Title] = FONT_KEY_GOTHIC_36_BOLD,
      [TextStyleFont_Body] = FONT_KEY_GOTHIC_36,
#endif
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_28,
      [TextStyleFont_Caption] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_Footer] = FONT_KEY_GOTHIC_24,
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_28,
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_24_BOLD,
#if !RECOVERY_FW
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
#endif
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_18_BOLD,
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_18_BOLD,
    },
  },
};

////////////////////
// Helpers

static const char *prv_get_font_for_size(PreferredContentSize content_size, TextStyleFont font) {
  if (content_size >= NumPreferredContentSizes) {
    PBL_LOG(LOG_LEVEL_ERROR, "Requested a content size that is out of bounds (%d)", content_size);
    goto fail;
  } else if (font >= TextStyleFontCount) {
    PBL_LOG(LOG_LEVEL_ERROR, "Requested a style font that is out of bounds (%d)", font);
    goto fail;
  }
  return s_text_styles[content_size].fonts[font];
fail:
  PRIVILEGE_WAS_ELEVATED ? syscall_failed() : WTF;
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
DEFINE_SYSCALL(const char *, system_theme_get_font_key, TextStyleFont font) {
  return prv_get_font_for_size(system_theme_get_content_size(), font);
}

// *** WARNING WARNING WARNING ***
// Be very careful when modifying this syscall. It currently returns a pointer
// to constant data in flash, which unprivileged apps are allowed to read.
DEFINE_SYSCALL(const char *, system_theme_get_font_key_for_size, PreferredContentSize content_size,
               TextStyleFont font) {
  const PreferredContentSize size_on_runtime_platform =
      system_theme_convert_host_content_size_to_runtime_platform(content_size);
  return prv_get_font_for_size(size_on_runtime_platform, font);
}

GFont system_theme_get_font(TextStyleFont font) {
  return fonts_get_system_font(system_theme_get_font_key(font));
}

GFont system_theme_get_font_for_size(PreferredContentSize size, TextStyleFont font) {
  return fonts_get_system_font(system_theme_get_font_key_for_size(size, font));
}

GFont system_theme_get_font_for_default_size(TextStyleFont font) {
  return fonts_get_system_font(system_theme_get_font_key_for_size(PreferredContentSizeDefault,
                                                                  font));
}

static const PreferredContentSize s_platform_default_content_sizes[] = {
  [PlatformTypeAplite] = PreferredContentSizeMedium,
  [PlatformTypeBasalt] = PreferredContentSizeMedium,
  [PlatformTypeChalk] = PreferredContentSizeMedium,
  [PlatformTypeDiorite] = PreferredContentSizeMedium,
  [PlatformTypeEmery] = PreferredContentSizeLarge,
  [PlatformTypeFlint] = PreferredContentSizeMedium,
};

T_STATIC PreferredContentSize prv_convert_content_size_between_platforms(PreferredContentSize size,
                                                                         PlatformType from_platform,
                                                                         PlatformType to_platform) {
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
                                                    PBL_PLATFORM_TYPE_CURRENT,
                                                    runtime_platform);
}

PreferredContentSize system_theme_convert_host_content_size_to_runtime_platform(
    PreferredContentSize size) {
  const PlatformType runtime_platform = process_manager_current_platform();
  return prv_convert_content_size_between_platforms(size, PBL_PLATFORM_TYPE_CURRENT,
                                                    runtime_platform);
}

void analytics_external_collect_system_theme_settings(void) {
  const SettingsContentSize content_size =
      settings_content_size_from_preferred_size(system_theme_get_content_size());
  analytics_set(ANALYTICS_DEVICE_METRIC_SYSTEM_THEME_TEXT_STYLE, content_size,
                AnalyticsClient_System);
}

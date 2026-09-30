/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system_theme.h"
#include "system_theme_style.h"

#include "applib/fonts/fonts.h"
#include "process_management/process_manager.h"
#include "syscall/syscall_internal.h"
#include "system/passert.h"
#include "pbl/util/size.h"
#include "pbl/util/testing.h"

////////////////////
// Helpers

static const char *prv_get_font_for_size(PreferredContentSize content_size, TextStyleFont font) {
  if (content_size >= NumPreferredContentSizes) {
    PBL_LOG_ERR("Requested a content size that is out of bounds (%d)", content_size);
    goto fail;
  } else if (font >= TextStyleFontCount) {
    PBL_LOG_ERR("Requested a style font that is out of bounds (%d)", font);
    goto fail;
  }
  return system_theme_get_style_for_size(content_size)->fonts[font];
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

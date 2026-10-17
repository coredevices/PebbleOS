/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system_theme.h"

#include "system_theme_style.h"

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/als.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>
#include <pbl/services/light.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>

#include <applib/fonts/fonts.h>
#include <process_management/pebble_process_md.h>
#include <process_management/process_manager.h>
#include <shell/prefs.h>
#include <syscall/syscall.h>

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

//! Returns true if the current (system) app context should render in dark mode.
static bool prv_is_system_app(void) {
  const PebbleTask task = pebble_task_get_current();
  if (task != PebbleTask_App && task != PebbleTask_Worker) {
    // Kernel / other tasks are always treated as system
    return true;
  }
  const ProcessAppSDKType sdk_type =
      process_metadata_get_app_sdk_type(sys_process_manager_get_current_process_md());
  return sdk_type == ProcessAppSDKType_System;
}

PBL_SYSCALL(bool, system_theme_is_system_ui) {
  return prv_is_system_app();
}

//! Last ambient light sample for DarkModeAmbient. Only written by system_theme_refresh_ambient()
//! because sampling the sensor can block, so rendering code reads this instead.
static volatile bool s_ambient_is_dark;

void system_theme_refresh_ambient(void) {
  s_ambient_is_dark = !ambient_light_is_light();
}

PBL_SYSCALL(bool, system_theme_is_dark_mode) {
  if (!prv_is_system_app()) {
    return false;
  }
  switch (shell_prefs_get_dark_mode()) {
    case DarkModeOff:
      return false;
    case DarkModeOn:
      return true;
    case DarkModeAmbient:
      return s_ambient_is_dark;
    case DarkModeScheduled: {
      DarkModeSchedule schedule;
      shell_prefs_get_dark_mode_schedule(&schedule);
      struct pbl_tm time_now;
      rtc_get_time_tm(&time_now);
      const int now_min = time_now.tm_hour * 60 + time_now.tm_min;
      const int start_min = schedule.from_hour * 60 + schedule.from_minute;
      const int end_min = schedule.to_hour * 60 + schedule.to_minute;
      if (start_min == end_min) {
        return false;
      }
      if (start_min < end_min) {
        return (now_min >= start_min && now_min < end_min);
      } else {
        return (now_min >= start_min || now_min < end_min);
      }
    }
    default:
      return false;
  }
}

GColor system_theme_get_bg_color(void) {
  return system_theme_is_dark_mode() ? GColorBlack : GColorWhite;
}

GColor system_theme_get_fg_color(void) {
  return system_theme_is_dark_mode() ? GColorWhite : GColorBlack;
}

PBL_SYSCALL(GColor, system_theme_get_highlight_color) {
  if (!prv_is_system_app()) {
    return GColorBlack;
  }
  return shell_prefs_get_theme_highlight_color();
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

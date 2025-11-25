/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_activity_tracker.h"
#include "settings_bluetooth.h"
#include "settings_display.h"
#include "settings_menu.h"
#include "settings_notifications.h"
#include "settings_quick_launch.h"
#include "settings_quiet_time.h"
#include "settings_remote.h"
#include "settings_system.h"
#include "settings_time.h"
#include "settings_timeline.h"
#include "settings_themes.h"

#if CAPABILITY_HAS_VIBE_SCORES
#include "settings_vibe_patterns.h"
#endif

#include "applib/ui/app_window_stack.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "shell/prefs.h"

static const SettingsModuleGetMetadata s_submodule_registry[] = {
  [SettingsMenuItemBluetooth]     = settings_bluetooth_get_info,
  [SettingsMenuItemNotifications] = settings_notifications_get_info,
#if CAPABILITY_HAS_VIBE_SCORES
  [SettingsMenuItemVibrations]    = settings_vibe_patterns_get_info,
#endif
  [SettingsMenuItemQuietTime]     = settings_quiet_time_get_info,
#if CAPABILITY_HAS_TIMELINE_PEEK
  [SettingsMenuItemTimeline]      = settings_timeline_get_info,
#endif
#if !TINTIN_FORCE_FIT
  [SettingsMenuItemActivity]      = settings_activity_tracker_get_info,
  [SettingsMenuItemQuickLaunch]   = settings_quick_launch_get_info,
  [SettingsMenuItemDateTime]      = settings_time_get_info,
#else
  [SettingsMenuItemActivity]      = settings_system_get_info,
  [SettingsMenuItemQuickLaunch]   = settings_system_get_info,
  [SettingsMenuItemDateTime]      = settings_system_get_info,
#endif
  [SettingsMenuItemDisplay]       = settings_display_get_info,
#if PBL_COLOR
  [SettingsMenuItemThemes]        = settings_themes_get_info,
#endif
  [SettingsMenuItemSystem]        = settings_system_get_info,
};

const SettingsModuleMetadata *settings_menu_get_submodule_info(SettingsMenuItem category) {
  PBL_ASSERTN(category < SettingsMenuItem_Count);
  return s_submodule_registry[category]();
}

const char *settings_menu_get_status_name(SettingsMenuItem category) {
  const SettingsModuleMetadata *info = settings_menu_get_submodule_info(category);
  return info->name;
}

void settings_menu_push(SettingsMenuItem category) {
  Window *window = settings_menu_get_submodule_info(category)->init();
  app_window_stack_push(window, true /* animated */);
}

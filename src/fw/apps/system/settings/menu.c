/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "activity_tracker.h"
#include "bluetooth.h"
#include "display.h"
#include "menu.h"
#include "notifications.h"
#include "quick_launch.h"
#include "quiet_time.h"
#include "remote.h"
#include "system.h"
#include "time.h"
#include "timeline.h"
#if CAPABILITY_HAS_THEMING
#include "themes.h"
#endif
#include "health.h"
#include "vibe_patterns.h"

#include "applib/ui/app_window_stack.h"
#include "pbl/services/i18n/i18n.h"
#include "system/passert.h"
#include "shell/prefs.h"

static const SettingsModuleGetMetadata s_submodule_registry[] = {
  [SettingsMenuItemBluetooth]     = settings_bluetooth_get_info,
  [SettingsMenuItemNotifications] = settings_notifications_get_info,
  [SettingsMenuItemVibrations]    = settings_vibe_patterns_get_info,
  [SettingsMenuItemQuietTime]     = settings_quiet_time_get_info,
#if CAPABILITY_HAS_TIMELINE_PEEK
  [SettingsMenuItemTimeline]      = settings_timeline_get_info,
#endif
  [SettingsMenuItemHealth]        = settings_health_get_info,
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
#if CAPABILITY_HAS_THEMING
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

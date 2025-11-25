/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system_app_ids.auto.h"
#include "resource/resource_ids.auto.h"

extern const PebbleProcessMd *simplicity_get_app_info(void);
extern const PebbleProcessMd *low_power_face_get_app_info(void);
extern const PebbleProcessMd *music_app_get_info(void);
extern const PebbleProcessMd *notifications_app_get_info(void);
extern const PebbleProcessMd *alarms_app_get_info(void);
extern const PebbleProcessMd *watchfaces_get_app_info(void);
extern const PebbleProcessMd *settings_get_app_info(void);
extern const PebbleProcessMd *set_time_get_app_info(void);
extern const PebbleProcessMd *quick_launch_setup_get_app_info(void);
extern const PebbleProcessMd *timeline_get_app_info(void);
extern const PebbleProcessMd *launcher_menu_app_get_app_info(void);;
extern const PebbleProcessMd *weather_app_get_info(void);
extern const PebbleProcessMd *shutdown_charging_get_app_info(void);
extern const PebbleProcessMd *battery_critical_get_app_info(void);


static const AppRegistryEntry APP_RECORDS[] = {

  // System Apps
  {
    .id = APP_ID_SIMPLICITY,
    .type = AppInstallStorageFw,
    .md_fn = &simplicity_get_app_info
  },
  {
    .id = APP_ID_LOW_POWER_FACE,
    .type = AppInstallStorageFw,
    .md_fn = &low_power_face_get_app_info
  },
  {
    .id = APP_ID_MUSIC,
    .type = AppInstallStorageFw,
    .md_fn = &music_app_get_info
  },
  {
    .id = APP_ID_NOTIFICATIONS,
    .type = AppInstallStorageFw,
    .md_fn = &notifications_app_get_info
  },
  {
    .id = APP_ID_ALARMS,
    .type = AppInstallStorageFw,
    .md_fn = &alarms_app_get_info
  },
  {
    .id = APP_ID_WATCHFACES,
    .type = AppInstallStorageFw,
    .md_fn = &watchfaces_get_app_info
  },
  {
    .id = APP_ID_SETTINGS,
    .type = AppInstallStorageFw,
    .md_fn = &settings_get_app_info
  },
  {
    .id = APP_ID_SET_TIME,
    .type = AppInstallStorageFw,
    .md_fn = &set_time_get_app_info
  },
  {
    .id = APP_ID_QUICK_LAUNCH_SETUP,
    .type = AppInstallStorageFw,
    .md_fn = &quick_launch_setup_get_app_info
  },
  {
    .id = APP_ID_TIMELINE,
    .type = AppInstallStorageFw,
    .md_fn = &timeline_get_app_info
  },
  {
    .id = APP_ID_VOICE_UI,
    .type = AppInstallStorageFw,
    .md_fn = &voice_ui_app_get_info
  },
  {
    .id = APP_ID_LAUNCHER_MENU,
    .type = AppInstallStorageFw,
    .md_fn = &launcher_menu_app_get_app_info
  },
  {
    .id = APP_ID_LIGHT_CONFIG,
    .type = AppInstallStorageFw,
    .md_fn = &light_config_get_info
  },
  {
    .id = APP_ID_AMB_LIGHT_READ,
    .type = AppInstallStorageFw,
    .md_fn = &ambient_light_reading_get_info
  },
  {
    .id = APP_ID_WEATHER,
    .type = AppInstallStorageFw,
    .md_fn = &weather_app_get_info
  },
  {
    .id = APP_ID_SHUTDOWN_CHARGING,
    .type = AppInstallStorageFw,
    .md_fn = &shutdown_charging_get_app_info
  },
  {
    .id = APP_ID_BATTERY_CRITICAL,
    .type = AppInstallStorageFw,
    .md_fn = &battery_critical_get_app_info
  },
  {
    .id = APP_ID_HEALTH_APP,
    .type = AppInstallStorageFw,
    .md_fn = &health_app_get_info
  },
  {
    .id = APP_ID_SPORTS,
    .type = AppInstallStorageFw,
    .md_fn = &sports_app_get_info
  },

  // Resource (stored) Apps
  {
    .id = APP_ID_GOLF,
    .type = AppInstallStorageResources,
    .name = "Golf",
    .uuid = { 0xcf, 0x1e, 0x81, 0x6a, 0x9d, 0xb0, 0x45, 0x11, 0xbb, 0xb8, 0xf6, 0x0c, 0x48, 0xca, 0x8f, 0xac },
    .bin_resource_id = RESOURCE_ID_STORED_APP_GOLF,
    .icon_resource_id = RESOURCE_ID_LAUNCHER_ICON_GOLF
  }
};

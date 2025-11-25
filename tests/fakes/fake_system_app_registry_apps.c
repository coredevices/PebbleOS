/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_system_app_registry_apps.h"
#include "resource/resource_ids.auto.h"

const PebbleProcessMd* tictoc_get_app_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      // UUID: 8f3c8686-31a1-4f5f-91f5-01600c9bdc59
      .uuid = {0x8f, 0x3c, 0x86, 0x86, 0x31, 0xa1, 0x4f, 0x5f,
               0x91, 0xf5, 0x01, 0x60, 0x0c, 0x9b, 0xdc, 0x59},
      .process_type = ProcessTypeWatchface
    },
    .icon_resource_id = RESOURCE_ID_MENU_ICON_TICTOC_WATCH,
    .name = "TicToc"
  };
  return (const PebbleProcessMd*) &s_app_md;
}

const PebbleProcessMd *music_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      // UUID: 1f03293d-47af-4f28-b960-f2b02a6dd757
      .uuid = {0x1f, 0x03, 0x29, 0x3d, 0x47, 0xaf, 0x4f, 0x28,
               0xb9, 0x60, 0xf2, 0xb0, 0x2a, 0x6d, 0xd7, 0x57},
    },
    .name = "Music",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

const PebbleProcessMd* notifications_app_get_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      // UUID: b2cae818-10f8-46df-ad2b-98ad2254a3c1
      .uuid = {0xb2, 0xca, 0xe8, 0x18, 0x10, 0xf8, 0x46, 0xdf,
               0xad, 0x2b, 0x98, 0xad, 0x22, 0x54, 0xa3, 0xc1},
    },
    .name = "Notifications",
  };
  return (const PebbleProcessMd*) &s_app_md;
}

const PebbleProcessMd* watchfaces_get_app_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      // UUID: 18e443ce-38fd-47c8-84d5-6d0c775fbe55
      .uuid = {0x18, 0xe4, 0x43, 0xce, 0x38, 0xfd, 0x47, 0xc8,
               0x84, 0xd5, 0x6d, 0x0c, 0x77, 0x5f, 0xbe, 0x55},
    },
    .name = "Watchfaces",
  };
  return (const PebbleProcessMd*) &s_app_md;
}

const PebbleProcessMd* alarms_app_get_info() {
  static const PebbleProcessMdSystem s_alarms_app_info = {
    .common = {
      // UUID: 67a32d95-ef69-46d4-a0b9-854cc62f97f9
      .uuid = {0x67, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4,
               0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9},
    },
    .name = "Alarms",
  };
  return (const PebbleProcessMd*) &s_alarms_app_info;
}

const PebbleProcessMd* settings_get_app_info() {
  static const PebbleProcessMdSystem s_settings_app = {
    .common = {
      // UUID: 07e0d9cb-8957-4bf7-9d42-35bf47caadfe
      .uuid = {0x07, 0xe0, 0xd9, 0xcb, 0x89, 0x57, 0x4b, 0xf7,
               0x9d, 0x42, 0x35, 0xbf, 0x47, 0xca, 0xad, 0xfe},
    },
    .name = "Settings",
  };
  return (const PebbleProcessMd*) &s_settings_app;
}

const PebbleProcessMd *quiet_time_toggle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      // UUID: 2220d805-cf9a-4e12-92b9-5ca778aff6bb
      .uuid = {0x22, 0x20, 0xd8, 0x05, 0xcf, 0x9a, 0x4e, 0x12,
               0x92, 0xb9, 0x5c, 0xa7, 0x78, 0xaf, 0xf6, 0xbb},
      .visibility = ProcessVisibilityQuickLaunch,
    },
    .name = "Quiet Time",
  };
  return &s_app_info.common;
}

const PebbleProcessMd *workout_app_get_info(void) {
  static const PebbleProcessMdSystem s_workout_app_info = {
    .common = {
      .uuid = {0xfe, 0xf8, 0x2c, 0x82, 0x71, 0x76, 0x4e, 0x22,
               0x88, 0xde, 0x35, 0xa3, 0xfc, 0x18, 0xd4, 0x3f},
    },
    .name = "Workout",
  };
  return (const PebbleProcessMd*) &s_workout_app_info;
}

const PebbleProcessMd *sports_app_get_info(void) {
  static const PebbleProcessMdSystem s_sports_app_info = {
    .common = {
      .uuid = {0x4d, 0xab, 0x81, 0xa6, 0xd2, 0xfc, 0x45, 0x8a,
               0x99, 0x2c, 0x7a, 0x1f, 0x3b, 0x96, 0xa9, 0x70},
      .visibility = ProcessVisibilityShownOnCommunication
    },
    .name = "Sports",
  };
  return (const PebbleProcessMd*) &s_sports_app_info;
}

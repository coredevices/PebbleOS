/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/backlight.h"
#include "process_management/pebble_process_md.h"
#include "services/normal/activity/activity.h"
#include "services/normal/timeline/peek.h"
#include "shell/prefs.h"
#include "util/uuid.h"

#include <stdlib.h>

void app_idle_timeout_start(void) {
}

void app_idle_timeout_refresh(void) {
}

void app_idle_timeout_stop(void) {
}

void watchface_start_low_power(bool enable) {
}

uint32_t backlight_get_timeout_ms(void) {
  return DEFAULT_BACKLIGHT_TIMEOUT_MS;
}

#define SDKSHELL_BACKLIGHT_ON_PERCENT 25 // Same as snowy bb2

uint16_t backlight_get_intensity(void) {
  return (BACKLIGHT_BRIGHTNESS_MAX * SDKSHELL_BACKLIGHT_ON_PERCENT / 100);
}
uint8_t backlight_get_intensity_percent(void) {
  return (backlight_get_intensity() * 100) / BACKLIGHT_BRIGHTNESS_MAX;
}

void backlight_set_timeout_ms(uint32_t timeout_ms) {
}

BacklightBehaviour backlight_get_behaviour(void) {
  return BacklightBehaviour_On;
}

bool backlight_is_enabled(void) {
  return true;
}

bool backlight_is_ambient_sensor_enabled(void) {
  return false;
}

bool backlight_is_motion_enabled(void) {
  return false;
}

#include "process_management/app_install_types.h"
void worker_preferences_set_default_worker(AppInstallId id) {
}

AppInstallId worker_preferences_get_default_worker(void) {
  return INSTALL_ID_INVALID;
}

#if !CAPABILITY_HAS_SDK_SHELL4
const char *app_custom_get_title(AppInstallId id) {
  return NULL;
}

void customizable_app_protocol_msg_callback(void *session, const uint8_t* data,
                                            size_t length) {
}
#endif

// Used by the alarm service to add alarm pins to the timeline
const PebbleProcessMd* alarms_app_get_info(void) {
  static const PebbleProcessMdSystem s_alarms_app_info = {
    .common = {
      .main_func = NULL,
      // UUID: 67a32d95-ef69-46d4-a0b9-854cc62f97f9
      .uuid = {0x67, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4,
               0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9},
    },
    .name = "Alarms",
  };
  return (const PebbleProcessMd*) &s_alarms_app_info;
}

// This stub isn't needed on tintin as it uses a different launcher
#if !PLATFORM_TINTIN && !CAPABILITY_HAS_SDK_SHELL4
#include "apps/system_apps/launcher/launcher_app.h"
const LauncherDrawState *launcher_app_get_draw_state(void) {
  return NULL;
}
#endif

void analytics_external_update(void) {
}

bool shell_prefs_get_stationary_enabled(void) {
  return false;
}

bool shell_prefs_get_language_english(void) {
  return true;
}
void shell_prefs_set_language_english(bool english) {
}
void shell_prefs_toggle_language_english(void) {
}

void language_ui_display_changed(const char *lang_name) {
}

void timeline_peek_prefs_set_enabled(bool enabled) {}
bool timeline_peek_prefs_get_enabled(void) {
  return true;
}
void timeline_peek_prefs_set_before_time(uint16_t before_time_m) {}
uint16_t timeline_peek_prefs_get_before_time(void) {
  return (TIMELINE_PEEK_DEFAULT_SHOW_BEFORE_TIME_S / SECONDS_PER_MINUTE);
}

bool workout_utils_find_ongoing_activity_session(ActivitySession *session_out) {
  return false;
}

uint8_t activity_prefs_heart_get_resting_hr(void) {
  return 70;
}

uint8_t activity_prefs_heart_get_elevated_hr(void) {
  return 100;
}

uint8_t activity_prefs_heart_get_max_hr(void) {
  return 190;
}

uint8_t activity_prefs_heart_get_zone1_threshold(void) {
  return 130;
}

uint8_t activity_prefs_heart_get_zone2_threshold(void) {
  return 154;
}

uint8_t activity_prefs_heart_get_zone3_threshold(void) {
  return 172;
}

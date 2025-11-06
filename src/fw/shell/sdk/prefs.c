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

#include "applib/preferred_content_size.h"
#include "apps/system_app_ids.h"
#include "board/board.h"
#include "os/mutex.h"
#include "process_management/app_install_manager.h"
#include "process_management/process_manager.h"
#include "services/normal/activity/activity.h"
#include "services/normal/activity/activity_insights.h"
#include "services/normal/activity/insights_settings.h"
#include "services/normal/settings/settings_file.h"
#include "shell/prefs.h"
#include "shell/prefs_private.h"

static PebbleMutex *s_mutex;

#define PREF_KEY_CLOCK_24H "clock24h"
static bool s_is_24h_style;

#define PREF_KEY_DEFAULT_WATCHFACE "watchface"
static Uuid s_default_watchface = UUID_INVALID_INIT;

#define PREF_KEY_CONTENT_SIZE "contentSize"
static uint8_t s_content_size;
#if !UNITTEST
_Static_assert(sizeof(PreferredContentSize) == sizeof(s_content_size),
               "sizeof(PreferredContentSize) grew, pref needs to be migrated!");
#endif

void shell_prefs_init(void) {
  s_mutex = mutex_create();
  mutex_lock(s_mutex);
  SettingsFile file = {};
  if (settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN) != S_SUCCESS) {
    goto cleanup;
  }
  if (settings_file_get(&file, PREF_KEY_CLOCK_24H, sizeof(PREF_KEY_CLOCK_24H),
                        &s_is_24h_style, sizeof(s_is_24h_style)) != S_SUCCESS) {
    // The setting likely doesn't exist yet so set it to the default (true)
    s_is_24h_style = true;
  }
  if (settings_file_get(&file, PREF_KEY_DEFAULT_WATCHFACE, sizeof(PREF_KEY_DEFAULT_WATCHFACE),
                        &s_default_watchface, sizeof(s_default_watchface)) != S_SUCCESS) {
    s_default_watchface = UUID_INVALID;
  }
  if (settings_file_get(&file, PREF_KEY_CONTENT_SIZE, sizeof(PREF_KEY_CONTENT_SIZE),
                        &s_content_size, sizeof(s_content_size)) != S_SUCCESS) {
    s_content_size = PreferredContentSizeDefault;
  }
  settings_file_close(&file);
cleanup:
  mutex_unlock(s_mutex);
}

static bool prv_pref_set(const char *key, const void *val, size_t val_len) {
  SettingsFile file = {};
  status_t rv;
  if ((rv = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN)) != S_SUCCESS) {
    goto cleanup;
  }
  rv = settings_file_set(&file, key, strnlen(key, SETTINGS_KEY_MAX_LEN) + 1, val, val_len);
  settings_file_close(&file);
cleanup:
  return (rv == S_SUCCESS);
}

bool shell_prefs_get_clock_24h_style(void) {
  return s_is_24h_style;
}

void shell_prefs_set_clock_24h_style(bool is_24h_style) {
  mutex_lock(s_mutex);
  if (prv_pref_set(PREF_KEY_CLOCK_24H, &s_is_24h_style, sizeof(s_is_24h_style))) {
    s_is_24h_style = is_24h_style;
  }
  mutex_unlock(s_mutex);
}

bool shell_prefs_is_timezone_source_manual(void) {
  // Force things to automatic
  return false;
}

void shell_prefs_set_timezone_source_manual(bool manual) {
}

int16_t shell_prefs_get_automatic_timezone_id(void) {
  // Invalid
  return -1;
}

void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
}


// Exported function used by blob_db API to set the backing store for a specific key.
// Not used by the SDK shell
bool prefs_private_write_backing(const uint8_t *key, size_t key_len, const void *value,
                               int value_len) {
  return false;
}


// Exported function used by blob_db API to get the length of a value in our backing store
// Not used by the SDK shell
int prefs_private_get_backing_len(const uint8_t *key, size_t key_len) {
  return 0;
}


// Exported function used by blob_db API to read our backing store
// Not used by the SDK shell
bool prefs_private_read_backing(const uint8_t *key, size_t key_len, void *value, int value_len) {
  return false;
}

#if CAPABILITY_HAS_SDK_SHELL4
void watchface_set_default_install_id(AppInstallId app_id) {
  mutex_lock(s_mutex);
  Uuid uuid;
  app_install_get_uuid_for_install_id(app_id, &uuid);
  if (prv_pref_set(PREF_KEY_DEFAULT_WATCHFACE, &uuid, sizeof(uuid))) {
    s_default_watchface = uuid;
  }
  mutex_unlock(s_mutex);
}

static bool prv_set_default_any_watchface_enumerate_callback(AppInstallEntry *entry, void *data) {
  if (!app_install_entry_is_watchface(entry) ||
      app_install_entry_is_hidden(entry)) {
    return true; // continue search
  }

  watchface_set_default_install_id(entry->install_id);
  return false;
}

AppInstallId watchface_get_default_install_id(void) {
  AppInstallId app_id = app_install_get_id_for_uuid(&s_default_watchface);
  AppInstallEntry entry;
  if ((app_id == INSTALL_ID_INVALID) ||
      !app_install_get_entry_for_install_id(app_id, &entry) ||
      !app_install_entry_is_watchface(&entry)) {
    app_install_enumerate_entries(prv_set_default_any_watchface_enumerate_callback, NULL);
    app_id = app_install_get_id_for_uuid(&s_default_watchface);
  }
  return app_id;
}
#else
AppInstallId watchface_get_default_install_id(void) {
  return APP_ID_SDK;
}

void watchface_set_default_install_id(AppInstallId id) {
}
#endif

void system_theme_set_content_size(PreferredContentSize content_size) {
  mutex_lock(s_mutex);
  const uint8_t content_size_uint = content_size;
  if (content_size >= NumPreferredContentSizes) {
    PBL_LOG(LOG_LEVEL_WARNING, "Ignoring attempt to set content size to invalid size %d",
            content_size);
  } else if (prv_pref_set(PREF_KEY_CONTENT_SIZE, &content_size_uint, sizeof(content_size_uint))) {
    s_content_size = content_size;
  }
  mutex_unlock(s_mutex);
}

PreferredContentSize system_theme_get_content_size(void) {
  return system_theme_convert_host_content_size_to_runtime_platform(
      (PreferredContentSize)s_content_size);
}

bool activity_prefs_tracking_is_enabled(void) {
#if CAPABILITY_HAS_HEALTH_TRACKING
  return true;
#else
  return false;
#endif
}

#if CAPABILITY_HAS_HEALTH_TRACKING
void activity_prefs_tracking_set_enabled(bool enable) {
}

bool activity_prefs_activity_insights_are_enabled(void) {
  return false;
}

void activity_prefs_activity_insights_set_enabled(bool enable) {
}

bool activity_prefs_sleep_insights_are_enabled(void) {
  return false;
}

void activity_prefs_sleep_insights_set_enabled(bool enable) {
}

uint8_t activity_prefs_get_health_app_opened_version(void) {
  return 0;
}

void activity_prefs_set_height_mm(uint16_t height_mm) {
}

uint16_t activity_prefs_get_height_mm(void) {
  return 0;
}

void activity_prefs_set_weight_dag(uint16_t weight_dag) {
}

uint16_t activity_prefs_get_weight_dag(void) {
  return 0;
}

void activity_prefs_set_gender(ActivityGender gender) {
}

ActivityGender activity_prefs_get_gender(void) {
  return ActivityGenderOther;
}

void activity_prefs_set_age_years(uint8_t age_years) {
}

uint8_t activity_prefs_get_age_years(void) {
  return 0;
}

bool activity_prefs_heart_rate_is_enabled(void) {
  return true;
}

ActivityInsightSettings *activity_prefs_get_sleep_reward_settings(void) {
  static ActivityInsightSettings s_settings = { 0 };
  return &s_settings;
}

void activity_prefs_set_activation_delay_insight_fired(ActivationDelayInsightType type) {
}

bool activity_prefs_has_activation_delay_insight_fired(ActivationDelayInsightType type) {
  return false;
}

bool activity_prefs_get_health_app_opened(void) {
  return false;
}

void activity_prefs_set_activated(void) {
}

time_t activity_prefs_get_activation_time(void) {
  return 0;
}

UnitsDistance shell_prefs_get_units_distance(void) {
  return UnitsDistance_Miles;
}

#endif

GColor shell_prefs_get_settings_menu_highlight_color(void) {
  return PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack);
}

void shell_prefs_set_settings_menu_highlight_color(GColor color) {
  // Not used in SDK shell
}

GColor shell_prefs_get_apps_menu_highlight_color(void) {
  return PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack);
}

void shell_prefs_set_apps_menu_highlight_color(GColor color) {
  // Not used in SDK shell
}

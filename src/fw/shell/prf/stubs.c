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

//! @file stubs.c
//!
//! This file stubs out functionality that we don't want in PRF. Ideally this file wouldn't have
//! to exist because systems that were common to both PRF and normal firmware wouldn't try to
//! use something that only exists in normal, but we're not quite there yet.

#include "util/uuid.h"
#include "drivers/backlight.h"
#include "kernel/events.h"
#include "popups/crashed_ui.h"
#include "popups/notifications/notification_window.h"
#include "process_management/app_install_manager.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_storage.h"
#include "resource/resource_storage_file.h"
#include "services/common/light.h"
#include "services/normal/notifications/do_not_disturb.h"
#include "services/normal/notifications/alerts_private.h"
#include "services/normal/persist.h"
#include "shell/prefs.h"

void app_fetch_binaries(const Uuid *uuid, AppInstallId app_id, bool has_worker) {
}

const char *app_custom_get_title(AppInstallId app_id) {
  return NULL;
}

void crashed_ui_show_worker_crash(AppInstallId install_id) {
}

void crashed_ui_show_pebble_reset(void) {
}

void crashed_ui_show_bluetooth_stuck(void) {
}

void app_idle_timeout_stop(void) {
}

void wakeup_popup_window(uint8_t missed_apps_count, uint8_t *missed_apps_banks) {
}

void watchface_set_default_install_id(AppInstallId id) {
}

void watchface_handle_button_event(PebbleEvent *e) {
}

void app_idle_timeout_refresh(void) {
}

PebblePhoneCaller* phone_call_util_create_caller(const char *number, const char *name) {
  return NULL;
}

void alarm_set_snooze_delay(int delay_ms) {
}

const void* const g_pbl_system_tbl[] = {};

const FileResourceData g_file_resource_stores[] = {};
const uint32_t g_num_file_resource_stores = 0;

void persist_service_client_open(const Uuid *uuid) {
}

void persist_service_client_close(const Uuid *uuid) {
}

SettingsFile * persist_service_lock_and_get_store(const Uuid *uuid) {
  return NULL;
}

status_t persist_service_delete_file(const Uuid *uuid) {
  return E_INVALID_OPERATION;
}

void wakeup_enable(bool enable) {
}

bool phone_call_is_using_ANCS(void) {
  return true;
}

#include "services/normal/notifications/alerts.h"

#include "services/normal/blob_db/app_db.h"
#include "services/normal/app_cache.h"
#include "services/normal/blob_db/pin_db.h"

status_t pin_db_delete_with_parent(const TimelineItemId *parent_id) {
  return E_INVALID_OPERATION;
}

status_t app_cache_add_entry(AppInstallId app_id, uint32_t total_size) {
  return E_INVALID_OPERATION;
}

status_t app_cache_remove_entry(AppInstallId id) {
  return E_INVALID_OPERATION;
}

status_t app_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return E_INVALID_OPERATION;
}

status_t app_db_delete(const uint8_t *key, int key_len) {
  return E_INVALID_OPERATION;
}

AppInstallId app_db_check_next_unique_id(void) {
  return 0;
}

void app_db_enumerate_entries(AppDBEnumerateCb cb, void *data) {
}

AppInstallId app_db_get_install_id_for_uuid(const Uuid *uuid) {
  return 0;
}

status_t app_db_get_app_entry_for_install_id(AppInstallId app_id, AppDBEntry *entry) {
  return E_INVALID_OPERATION;
}

bool app_db_exists_install_id(AppInstallId app_id) {
  return false;
}

void timeline_item_destroy(TimelineItem* item) {
}

AppInstallId worker_preferences_get_default_worker(void) {
  return INSTALL_ID_INVALID;
}

#include "process_management/process_loader.h"
void * process_loader_load(const PebbleProcessMd *app_md, PebbleTask task,
                         MemorySegment *destination) {
  return app_md->main_func;
}

#include "services/normal/process_management/app_storage.h"
AppStorageGetAppInfoResult app_storage_get_process_info(PebbleProcessInfo* app_info,
                                                        uint8_t *build_id_out,
                                                        AppInstallId app_id,
                                                        PebbleTask task) {
  return GET_APP_INFO_COULD_NOT_READ_FORMAT;
}

void app_storage_get_file_name(char *name, size_t buf_length, AppInstallId app_id,
                               PebbleTask task) {
  // Empty string
  *name = 0;
}

bool shell_prefs_get_clock_24h_style(void) {
  return true;
}

void shell_prefs_set_clock_24h_style(bool is_24h_style) {
}

bool shell_prefs_is_timezone_source_manual(void) {
  return false;
}

void shell_prefs_set_timezone_source_manual(bool manual) {
}

void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
}

int16_t shell_prefs_get_automatic_timezone_id(void) {
  return -1;
}

bool shell_prefs_can_coredump_on_request() {
  // it would be good to have a core dump escape hatch in PRF
  return true;
}

AlertMask alerts_get_mask(void) {
  return AlertMaskAllOff;
}

bool do_not_disturb_is_active(void) {
  return true;
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

bool bt_persistent_storage_get_airplane_mode_enabled(void) {
  return false;
}

void bt_persistent_storage_set_airplane_mode_enabled(bool *state) {
}

uint32_t backlight_get_timeout_ms(void) {
  return DEFAULT_BACKLIGHT_TIMEOUT_MS;
}

uint16_t backlight_get_intensity(void) {
  return BACKLIGHT_BRIGHTNESS_MAX;
}
uint8_t backlight_get_intensity_percent(void) {
  return (backlight_get_intensity() * 100) / BACKLIGHT_BRIGHTNESS_MAX;
}

bool shell_prefs_get_language_english(void) {
  return true;
}
void shell_prefs_set_language_english(bool english) {
}
void shell_prefs_toggle_language_english(void) {
}

FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height) {
  return NULL;
}

void analytics_event_app_crash(const Uuid *uuid, uint32_t pc, uint32_t lr,
                               const uint8_t *build_id, bool is_rocky_app) {
}

void analytics_event_bt_chip_boot(uint8_t build_id[BUILD_ID_EXPECTED_LEN],
                                  uint32_t crash_lr, uint32_t reboot_reason_code) {
}

int16_t timeline_peek_get_origin_y(void) {
  return DISP_ROWS;
}

int16_t timeline_peek_get_obstruction_origin_y(void) {
  return DISP_ROWS;
}

void timeline_peek_handle_process_start(void) { }

void timeline_peek_handle_process_kill(void) { }

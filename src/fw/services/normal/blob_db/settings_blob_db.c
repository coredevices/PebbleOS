/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_blob_db.h"
#include "sync.h"
#include "api.h"

#include "kernel/pbl_malloc.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/normal/settings/settings_file.h"
#include "shell/prefs_private.h"
#include "system/logging.h"
#include "system/passert.h"
#include "services/common/system_task.h"
#include "util/list.h"
#include "util/size.h"

#include <string.h>

//! Flag to suppress change callback during phone-originated INSERTs.
//! This prevents queuing unnecessary sync callbacks that would flood the system task queue.
static bool s_suppress_change_callback = false;

//! Flag to track if a sync callback is already pending.
//! This coalesces multiple setting changes into a single sync callback.
static bool s_sync_callback_pending = false;

//! Settings Whitelist
//!
//! Only these settings will be synced via BlobDB.
//! This prevents sensitive data (Bluetooth pairing, debug flags, etc.) from syncing.
static const char *s_syncable_settings[] = {
  // Clock preferences
  "clock24h",
  "timezoneSource",
  "automaticTimezoneID",

  // Display preferences
  "unitsDistance",
  "textStyle",
  "stationaryMode",
#if PLATFORM_ASTERIX
  "displayOrientationLeftHanded",
#endif

  // Motion preferences
#if CAPABILITY_HAS_ACCEL_SENSITIVITY
  "motionSensitivity",
#endif

  // Backlight preferences
  "lightEnabled",
  "lightAmbientSensorEnabled",
  "lightTimeoutMs",
  "lightIntensity",
  "lightMotion",
  "lightAmbientThreshold",
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  "lightDynamicIntensity",
  "dynBacklightMinThreshold",
  "dynBacklightMaxThreshold",
#endif

  // Language preferences
  "langEnglish",

  // App preferences
  "watchface",
  "qlUp",
  "qlDown",
  "qlSelect",
  "qlBack",
  "qlSetupOpened",
  "qlSingleClickUp",
  "qlSingleClickDown",

  // UI theming
  "settingsMenuHighlightColor",
  "appsMenuHighlightColor",

  // Timeline preferences
#if CAPABILITY_HAS_TIMELINE_PEEK
  "timelineQuickViewEnabled",
  "timelineQuickViewBeforeTimeMin",
  "timelineSettingsOpened",
#endif

  // Activity preferences
#if CAPABILITY_HAS_HEALTH_TRACKING
  "activityPreferences",
  "activityHealthAppOpened",
  "activityWorkoutAppOpened",
  "alarmsAppOpened",
  "hrmPreferences",
  "heartRatePreferences",
#endif

  // Worker preferences
  "workerId",
};

static const size_t s_num_syncable_settings = ARRAY_LENGTH(s_syncable_settings);

static bool s_initialized = false;

//! Check if the connected phone supports Settings BlobDB sync
//! Returns true if the phone advertises settings_sync_support capability
bool settings_blob_db_phone_supports_sync(void) {
  PebbleProtocolCapabilities capabilities;
  bt_persistent_storage_get_cached_system_capabilities(&capabilities);
  return capabilities.settings_sync_support;
}

//! Check if a setting key is in the sync whitelist
static bool prv_is_syncable(const uint8_t *key, int key_len) {
  for (size_t i = 0; i < s_num_syncable_settings; i++) {
    const char *syncable_key = s_syncable_settings[i];
    int syncable_len = (int)(strlen(syncable_key) + 1); // Include null terminator
    if (key_len == syncable_len && memcmp(key, syncable_key, (size_t)key_len) == 0) {
      return true;
    }
  }
  return false;
}

//! Kernel background callback to sync all dirty settings.
//! This is coalesced - only one instance runs at a time.
static void prv_deferred_sync_callback(void *data) {
  s_sync_callback_pending = false;

  // Only sync if the phone supports settings sync
  if (!settings_blob_db_phone_supports_sync()) {
    return;
  }

  // Sync all dirty settings using the existing dirty list mechanism
  blob_db_sync_db(BlobDBIdSettings);
}

//! Callback for settings changes - defers sync to avoid re-entrancy
//! The callback is invoked while the settings file is still open, so we can't
//! immediately sync (which would try to open the file again). Instead, we defer
//! the sync to run after the file operations complete.
//!
//! This callback is coalesced - multiple setting changes result in at most one
//! queued callback, preventing system task queue overflow.
static void prv_settings_change_callback(SettingsFile *file, const void *key, int key_len,
                                         time_t last_modified) {
  // Skip callback if suppressed (during phone-originated INSERTs)
  if (s_suppress_change_callback) {
    return;
  }

  // Only sync whitelisted settings
  if (!prv_is_syncable((const uint8_t *)key, key_len)) {
    return;
  }

  // Coalesce multiple changes into a single callback to avoid queue overflow
  if (s_sync_callback_pending) {
    return;
  }

  s_sync_callback_pending = true;
  system_task_add_callback(prv_deferred_sync_callback, NULL);
}

// BlobDB Interface Implementation

void settings_blob_db_init(void) {
  if (s_initialized) {
    return;
  }

  // Register callback to sync settings immediately when they change
  settings_file_set_change_callback(prv_settings_change_callback);

  s_initialized = true;
  PBL_LOG(LOG_LEVEL_INFO, "Settings BlobDB initialized (%u whitelisted settings)",
          (unsigned int) s_num_syncable_settings);
}

status_t settings_blob_db_insert(const uint8_t *key, int key_len,
                                 const uint8_t *val, int val_len) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  // Only allow whitelisted settings to be synced
  if (!prv_is_syncable(key, key_len)) {
    char key_str[128];
    size_t copy_len = (key_len > 0 && (size_t)key_len < sizeof(key_str)) ?
                      (size_t)key_len : sizeof(key_str) - 1;
    memcpy(key_str, key, copy_len);
    key_str[copy_len] = '\0';
    PBL_LOG(LOG_LEVEL_WARNING, "Rejecting non-whitelisted setting: %s", key_str);
    return E_INVALID_OPERATION;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  // Suppress change callback - we don't want to sync back to phone for phone-originated INSERTs.
  // Keep suppressed through prefs_private_handle_blob_db_event() because validation failures
  // can trigger prefs_private_write_backing() which would fire the callback again.
  s_suppress_change_callback = true;
  status = settings_file_set(&file, key, key_len, val, val_len);

  if (PASSED(status)) {
    // Mark as synced - the record came from the phone so it's already in sync
    settings_file_mark_synced(&file, key, key_len);
  }
  settings_file_close(&file);

  // Update the in-memory prefs state after successful write
  if (PASSED(status)) {
    PebbleBlobDBEvent event = {
      .db_id = BlobDBIdSettings,
      .type = BlobDBEventTypeInsert,
      .key = (uint8_t *)key,
      .key_len = key_len,
    };
    prefs_private_handle_blob_db_event(&event);
  }

  s_suppress_change_callback = false;
  return status;
}

int settings_blob_db_get_len(const uint8_t *key, int key_len) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  int len = settings_file_get_len(&file, key, key_len);
  settings_file_close(&file);
  return len;
}

status_t settings_blob_db_read(const uint8_t *key, int key_len,
                               uint8_t *val_out, int val_len) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  status = settings_file_get(&file, key, key_len, val_out, val_len);
  settings_file_close(&file);
  return status;
}

status_t settings_blob_db_delete(const uint8_t *key, int key_len) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  // Only allow whitelisted settings to be deleted
  if (!prv_is_syncable(key, key_len)) {
    return E_INVALID_OPERATION;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  status = settings_file_delete(&file, key, key_len);
  settings_file_close(&file);
  return status;
}

// Dirty list management

typedef struct {
  BlobDBDirtyItem *dirty_list;
  BlobDBDirtyItem *dirty_list_tail;
} BuildDirtyListContext;

static bool prv_build_dirty_list_callback(SettingsFile *file,
                                          SettingsRecordInfo *info,
                                          void *context) {
  BuildDirtyListContext *ctx = (BuildDirtyListContext *)context;

  // Skip settings that are already synced
  if (!info->dirty) {
    return true;
  }

  // Read the key to check whitelist
  uint8_t key_buf[SETTINGS_KEY_MAX_LEN];
  info->get_key(file, key_buf, info->key_len);

  // Only include whitelisted settings
  if (!prv_is_syncable(key_buf, info->key_len)) {
    return true; // Skip, continue iteration
  }

  // Allocate dirty item
  BlobDBDirtyItem *item = kernel_malloc_check(sizeof(BlobDBDirtyItem) + info->key_len);
  list_init((ListNode *)item);
  item->last_updated = (time_t)info->last_modified;
  item->key_len = info->key_len;
  memcpy(item->key, key_buf, info->key_len);

  // Add to list
  if (ctx->dirty_list == NULL) {
    ctx->dirty_list = item;
    ctx->dirty_list_tail = item;
  } else {
    ctx->dirty_list_tail = (BlobDBDirtyItem *)list_append(
        (ListNode *)ctx->dirty_list_tail, (ListNode *)item);
  }

  return true; // Continue iteration
}

BlobDBDirtyItem *settings_blob_db_get_dirty_list(void) {
  if (!s_initialized) {
    return NULL;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return NULL;
  }

  BuildDirtyListContext ctx = { .dirty_list = NULL, .dirty_list_tail = NULL };
  settings_file_each(&file, prv_build_dirty_list_callback, &ctx);
  settings_file_close(&file);

  return ctx.dirty_list;
}

status_t settings_blob_db_mark_synced(const uint8_t *key, int key_len) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  status = settings_file_mark_synced(&file, key, key_len);
  settings_file_close(&file);
  return status;
}

status_t settings_blob_db_is_dirty(bool *is_dirty_out) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  // Quick check: iterate and return true on first dirty whitelisted setting
  typedef struct {
    bool found_dirty;
  } IsDirtyContext;

  bool is_dirty_callback(SettingsFile *file, SettingsRecordInfo *info, void *context) {
    IsDirtyContext *ctx = (IsDirtyContext *)context;

    if (!info->dirty) {
      return true; // Continue
    }

    // Check if whitelisted
    uint8_t key_buf[SETTINGS_KEY_MAX_LEN];
    info->get_key(file, key_buf, info->key_len);

    if (prv_is_syncable(key_buf, info->key_len)) {
      ctx->found_dirty = true;
      return false; // Stop iteration
    }

    return true; // Continue
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  IsDirtyContext ctx = { .found_dirty = false };
  settings_file_each(&file, is_dirty_callback, &ctx);
  settings_file_close(&file);

  *is_dirty_out = ctx.found_dirty;
  return S_SUCCESS;
}

status_t settings_blob_db_flush(void) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  // SettingsFile writes are already atomic, no explicit flush needed
  PBL_LOG(LOG_LEVEL_DEBUG, "Settings BlobDB flush (no-op for SettingsFile)");
  return S_SUCCESS;
}

status_t settings_blob_db_mark_all_dirty(void) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  PBL_LOG(LOG_LEVEL_INFO, "Marking all settings as dirty for full sync");

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  status = settings_file_mark_all_dirty(&file);
  settings_file_close(&file);
  return status;
}

// Context for finding last_modified timestamp
typedef struct {
  const uint8_t *search_key;
  int search_key_len;
  time_t last_modified;
  bool found;
} GetTimestampContext;

static bool prv_get_timestamp_callback(SettingsFile *file, SettingsRecordInfo *info,
                                       void *context) {
  GetTimestampContext *ctx = (GetTimestampContext *)context;

  if (info->key_len != ctx->search_key_len) {
    return true; // Continue
  }

  uint8_t key_buf[SETTINGS_KEY_MAX_LEN];
  info->get_key(file, key_buf, info->key_len);

  if (memcmp(key_buf, ctx->search_key, info->key_len) == 0) {
    ctx->last_modified = (time_t)info->last_modified;
    ctx->found = true;
    return false; // Stop iteration
  }

  return true; // Continue
}

status_t settings_blob_db_insert_with_timestamp(const uint8_t *key, int key_len,
                                                const uint8_t *val, int val_len,
                                                time_t timestamp) {
  if (!s_initialized) {
    return E_INTERNAL;
  }

  // Only allow whitelisted settings
  if (!prv_is_syncable(key, key_len)) {
    return E_INVALID_OPERATION;
  }

  SettingsFile file;
  status_t status = settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (FAILED(status)) {
    return status;
  }

  // Check if existing value has a newer timestamp
  GetTimestampContext ctx = {
    .search_key = key,
    .search_key_len = key_len,
    .last_modified = 0,
    .found = false,
  };
  settings_file_each(&file, prv_get_timestamp_callback, &ctx);

  if (ctx.found && ctx.last_modified > timestamp) {
    // Watch data is newer - reject the insert
    settings_file_close(&file);
    PBL_LOG(LOG_LEVEL_DEBUG, "Rejecting stale data: watch=%lu phone=%lu",
            (unsigned long)ctx.last_modified, (unsigned long)timestamp);
    return E_INVALID_OPERATION;
  }

  // Phone data is newer or equal, or key doesn't exist - do the insert
  // Suppress change callback - we don't want to sync back to phone for phone-originated INSERTs.
  // Keep suppressed through prefs_private_handle_blob_db_event() because validation failures
  // can trigger prefs_private_write_backing() which would fire the callback again.
  s_suppress_change_callback = true;
  status = settings_file_set(&file, key, key_len, val, val_len);

  if (PASSED(status)) {
    // Mark as synced - the record came from the phone so it's already in sync
    settings_file_mark_synced(&file, key, key_len);
  }
  settings_file_close(&file);

  // Update the in-memory prefs state after successful write
  if (PASSED(status)) {
    PebbleBlobDBEvent event = {
      .db_id = BlobDBIdSettings,
      .type = BlobDBEventTypeInsert,
      .key = (uint8_t *)key,
      .key_len = key_len,
    };
    prefs_private_handle_blob_db_event(&event);
  }

  s_suppress_change_callback = false;
  return status;
}

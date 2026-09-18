/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/blob_db/app_permissions_db.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "util/units.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_blob_db, CONFIG_SERVICE_BLOB_DB_LOG_LEVEL);

#define SETTINGS_FILE_NAME "app_permissions"
#define SETTINGS_FILE_SIZE KiBYTES(8)

static struct {
  SettingsFile settings_file;
  struct pbl_mutex mutex;
} s_app_permissions_db;

static status_t prv_lock_mutex_and_open_file(void) {
  pbl_mutex_lock(&s_app_permissions_db.mutex, PBL_FOREVER);
  status_t rv = settings_file_open_growable(&s_app_permissions_db.settings_file, SETTINGS_FILE_NAME,
                                            SETTINGS_FILE_SIZE, KiBYTES(2));
  if (rv != S_SUCCESS) {
    pbl_mutex_unlock(&s_app_permissions_db.mutex);
  }
  return rv;
}

static void prv_close_file_and_unlock_mutex(void) {
  settings_file_close(&s_app_permissions_db.settings_file);
  pbl_mutex_unlock(&s_app_permissions_db.mutex);
}

static bool prv_is_key_valid(int key_len) {
  return (key_len == UUID_SIZE);
}

static bool prv_is_val_valid(const uint8_t *val, int val_len) {
  if (!val || (val_len < (int)sizeof(AppPermissionsDBEntry))) {
    return false;
  }
  return (((const AppPermissionsDBEntry *)val)->version == APP_PERMISSIONS_DB_ENTRY_VERSION);
}

// Convenience API
////////////////////////////////////////////////////////////////////////////////

status_t app_permissions_db_get(const Uuid *uuid, AppPermissionsDBEntry *entry_out) {
  if (!uuid || !entry_out) {
    return E_INVALID_ARGUMENT;
  }
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }
  const int len = settings_file_get_len(&s_app_permissions_db.settings_file, uuid, sizeof(*uuid));
  if (len < (int)sizeof(*entry_out)) {
    rv = E_DOES_NOT_EXIST;
  } else {
    rv = settings_file_get(&s_app_permissions_db.settings_file, uuid, sizeof(*uuid), entry_out,
                           sizeof(*entry_out));
  }
  prv_close_file_and_unlock_mutex();
  return rv;
}

status_t app_permissions_db_set(const Uuid *uuid, const AppPermissionsDBEntry *entry) {
  if (!uuid || !entry) {
    return E_INVALID_ARGUMENT;
  }
  return blob_db_insert(BlobDBIdAppPermissions, (const uint8_t *)uuid, sizeof(*uuid),
                        (const uint8_t *)entry, sizeof(*entry));
}

status_t app_permissions_db_delete_for_uuid(const Uuid *uuid) {
  if (!uuid) {
    return E_INVALID_ARGUMENT;
  }
  return blob_db_delete(BlobDBIdAppPermissions, (const uint8_t *)uuid, sizeof(*uuid));
}

typedef struct {
  AppPermissionsDBEachCallback cb;
  void *context;
} EachContext;

static bool prv_each_record(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  EachContext *each = context;
  if ((info->key_len != UUID_SIZE) || (info->val_len < (int)sizeof(AppPermissionsDBEntry))) {
    return true;
  }
  Uuid uuid;
  AppPermissionsDBEntry entry;
  info->get_key(file, &uuid, sizeof(uuid));
  info->get_val(file, &entry, sizeof(entry));
  return each->cb(&uuid, &entry, each->context);
}

status_t app_permissions_db_each(AppPermissionsDBEachCallback cb, void *context) {
  if (!cb) {
    return E_INVALID_ARGUMENT;
  }
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }
  EachContext each = {.cb = cb, .context = context};
  rv = settings_file_each(&s_app_permissions_db.settings_file, prv_each_record, &each);
  prv_close_file_and_unlock_mutex();
  return rv;
}

// BlobDB APIs
////////////////////////////////////////////////////////////////////////////////

void app_permissions_db_init(void) {
  pbl_mutex_init(&s_app_permissions_db.mutex);
}

status_t app_permissions_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len) {
  if (!prv_is_key_valid(key_len)) {
    PBL_LOG_ERR("Error inserting app permission: invalid key");
    return E_INVALID_ARGUMENT;
  }
  if (!prv_is_val_valid(val, val_len)) {
    PBL_LOG_ERR("Error inserting app permission: invalid value");
    return E_INVALID_ARGUMENT;
  }

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv == S_SUCCESS) {
    rv = settings_file_set(&s_app_permissions_db.settings_file, key, key_len, val,
                           sizeof(AppPermissionsDBEntry));
    prv_close_file_and_unlock_mutex();
  }
  return rv;
}

int app_permissions_db_get_len(const uint8_t *key, int key_len) {
  if (!prv_is_key_valid(key_len)) {
    return 0;
  }
  int len = 0;
  if (prv_lock_mutex_and_open_file() == S_SUCCESS) {
    len = settings_file_get_len(&s_app_permissions_db.settings_file, key, key_len);
    prv_close_file_and_unlock_mutex();
  }
  return len;
}

status_t app_permissions_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len) {
  if (!prv_is_key_valid(key_len) || !val_out) {
    return E_INVALID_ARGUMENT;
  }
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv == S_SUCCESS) {
    rv = settings_file_get(&s_app_permissions_db.settings_file, key, key_len, val_out, val_out_len);
    prv_close_file_and_unlock_mutex();
  }
  return rv;
}

status_t app_permissions_db_delete(const uint8_t *key, int key_len) {
  if (!prv_is_key_valid(key_len)) {
    return E_INVALID_ARGUMENT;
  }
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv == S_SUCCESS) {
    rv = settings_file_delete(&s_app_permissions_db.settings_file, key, key_len);
    prv_close_file_and_unlock_mutex();
  }
  return rv;
}

status_t app_permissions_db_flush(void) {
  pbl_mutex_lock(&s_app_permissions_db.mutex, PBL_FOREVER);
  status_t rv = pfs_remove(SETTINGS_FILE_NAME);
  pbl_mutex_unlock(&s_app_permissions_db.mutex);
  return rv;
}

status_t app_permissions_db_compact(void) {
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }
  rv = settings_file_compact(&s_app_permissions_db.settings_file);
  prv_close_file_and_unlock_mutex();
  return rv;
}

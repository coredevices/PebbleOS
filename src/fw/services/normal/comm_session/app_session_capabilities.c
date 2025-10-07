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

#include "process_management/app_install_types.h"
#include "process_management/app_manager.h"
#include "services/common/comm_session/app_session_capabilities.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/settings/settings_file.h"
#include "util/units.h"

#define APP_SESSION_CAPABILITIES_CACHE_FILENAME "app_comm"

#define APP_SESSION_CAPABILITIES_CACHE_FILE_MAX_USED_SPACE (KiBYTES(2))

static status_t prv_open(SettingsFile *settings_file) {
  return settings_file_open(settings_file, APP_SESSION_CAPABILITIES_CACHE_FILENAME,
                            APP_SESSION_CAPABILITIES_CACHE_FILE_MAX_USED_SPACE);
}

bool comm_session_current_app_session_cache_has_capability(CommSessionCapability capability) {
  CommSession *app_session = comm_session_get_current_app_session();

  const Uuid app_uuid = app_manager_get_current_app_md()->uuid;

  SettingsFile settings_file;
  status_t open_status = prv_open(&settings_file);

  uint64_t cached_capabilities = 0;
  if (PASSED(open_status)) {
    settings_file_get(&settings_file,
                      &app_uuid, sizeof(app_uuid),
                      &cached_capabilities, sizeof(cached_capabilities));
  }

  uint64_t new_capabilities = cached_capabilities;
  if (app_session) {
    // Connected, grab fresh capabilities data:
    new_capabilities = comm_session_get_capabilities(app_session);

    if (FAILED(open_status)) {
      // File open failed, return live data without saving to cache
      goto done;
    }

    if (new_capabilities != cached_capabilities) {
      settings_file_set(&settings_file,
                        &app_uuid, sizeof(app_uuid),
                        &new_capabilities, sizeof(new_capabilities));
    }

  } else {
    // Not connected, use cached data.

    if (FAILED(open_status)) {
      // File open failed, no cache available
      goto done;
    }
  }
  settings_file_close(&settings_file);

done:
  return ((new_capabilities & capability) != 0);
}

static void prv_rewrite_cb(SettingsFile *old_file,
                           SettingsFile *new_file,
                           SettingsRecordInfo *info,
                           void *context) {
  if (!info->val_len) {
    return; // Cache for this app has been deleted, don't rewrite it
  }
  Uuid key;
  uint64_t val;
  info->get_key(old_file, &key, sizeof(key));
  info->get_val(old_file, &val, sizeof(val));
  settings_file_set(new_file, &key, sizeof(key), &val, sizeof(val));
}

void comm_session_app_session_capabilities_evict(const Uuid *app_uuid) {
  SettingsFile settings_file;
  if (PASSED(prv_open(&settings_file))) {
    settings_file_delete(&settings_file, app_uuid, sizeof(*app_uuid));
    settings_file_close(&settings_file);
  }
}

void comm_session_app_session_capabilities_init(void) {
  SettingsFile settings_file;
  status_t open_status = prv_open(&settings_file);
  if (PASSED(open_status)) {
    settings_file_rewrite(&settings_file, prv_rewrite_cb, NULL);
    settings_file_close(&settings_file);
  } else if (open_status == E_INVALID_OPERATION) {
    // File has invalid format (wrong magic or version), remove it and recreate
    pfs_remove(APP_SESSION_CAPABILITIES_CACHE_FILENAME);
    // Try to open again to create a fresh file
    if (PASSED(prv_open(&settings_file))) {
      settings_file_close(&settings_file);
    }
  }
}

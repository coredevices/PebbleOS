/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/blob_db/api.h"
#include "pbl/services/settings/settings_file.h"

bool sync_util_is_dirty_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  return false;
}

bool sync_util_build_dirty_list_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  return false;
}

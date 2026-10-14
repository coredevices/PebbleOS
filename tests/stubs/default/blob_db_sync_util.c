/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/api.h>
#include <pbl/services/settings/settings_file.h>

PBL_WEAK bool sync_util_is_dirty_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  return false;
}

PBL_WEAK bool sync_util_build_dirty_list_cb(SettingsFile *file, SettingsRecordInfo *info,
                                            void *context) {
  return false;
}

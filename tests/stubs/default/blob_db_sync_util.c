/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/api.h>
#include <pbl/settings/settings.h>

PBL_WEAK bool sync_util_is_dirty_cb(struct pbl_settings_file *file,
                                    struct pbl_settings_record *info, void *context) {
  return false;
}

PBL_WEAK bool sync_util_build_dirty_list_cb(struct pbl_settings_file *file,
                                            struct pbl_settings_record *info, void *context) {
  return false;
}

/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/blob_db/api.h>
#include <pbl/services/settings/settings_file.h>

/**
 * @defgroup services_blob_db_sync_util Sync helpers
 * @ingroup services_blob_db
 * @brief Settings file iteration helpers for databases that track dirty records.
 * @{
 */

/**
 * @brief Settings file each callback that detects a dirty record.
 *
 * @param file Settings file being iterated.
 * @param info Current record.
 * @param[in,out] context Pointer to a bool, set to true when a dirty record is found.
 * @return false once a dirty record is found, to stop iterating.
 */
bool sync_util_is_dirty_cb(SettingsFile *file, SettingsRecordInfo *info, void *context);

/**
 * @brief Settings file each callback that builds a dirty list.
 *
 * Stops iterating when memory runs out, leaving the list incomplete.
 *
 * @param file Settings file being iterated.
 * @param info Current record.
 * @param[in,out] context Pointer to a ::BlobDBDirtyItem list head, initially NULL.
 * @return false if out of memory, true otherwise.
 */
bool sync_util_build_dirty_list_cb(SettingsFile *file, SettingsRecordInfo *info, void *context);

/** @} */

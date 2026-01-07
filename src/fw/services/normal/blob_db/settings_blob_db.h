/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

//! Settings BlobDB - wraps SettingsFile to provide BlobDB interface
//!
//! This allows settings to sync using the existing BlobDB protocol,
//! so the mobile app can reuse its BlobDB sync implementation.
//!
//! Only whitelisted settings are synced (see settings_blob_db.c for list).

//! Initialize the settings BlobDB
void settings_blob_db_init(void);

//! Insert/update a setting
status_t settings_blob_db_insert(const uint8_t *key, int key_len,
                                 const uint8_t *val, int val_len);

//! Get the length of a setting value
int settings_blob_db_get_len(const uint8_t *key, int key_len);

//! Read a setting value
status_t settings_blob_db_read(const uint8_t *key, int key_len,
                               uint8_t *val_out, int val_len);

//! Delete a setting
status_t settings_blob_db_delete(const uint8_t *key, int key_len);

//! Get list of dirty (unsynced) settings
BlobDBDirtyItem *settings_blob_db_get_dirty_list(void);

//! Mark a setting as synced
status_t settings_blob_db_mark_synced(const uint8_t *key, int key_len);

//! Check if there are dirty settings
status_t settings_blob_db_is_dirty(bool *is_dirty_out);

//! Flush settings to disk
status_t settings_blob_db_flush(void);

//! Mark all whitelisted settings as dirty (unsynced)
//! This triggers a full sync of all settings to the phone
status_t settings_blob_db_mark_all_dirty(void);

//! Insert/update a setting only if the incoming timestamp is newer or equal
//! @param key the setting key
//! @param key_len length of the key
//! @param val the value to insert
//! @param val_len length of the value
//! @param timestamp the timestamp of the incoming data
//! @return S_SUCCESS if inserted, E_INVALID_OPERATION if watch data is newer (stale)
status_t settings_blob_db_insert_with_timestamp(const uint8_t *key, int key_len,
                                                const uint8_t *val, int val_len,
                                                time_t timestamp);

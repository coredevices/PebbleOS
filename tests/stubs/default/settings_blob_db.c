/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/api.h>

PBL_WEAK void settings_blob_db_init(void) {
}

PBL_WEAK int settings_blob_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                     int val_len) {
  return 0;
}

PBL_WEAK int settings_blob_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

PBL_WEAK int settings_blob_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return -ENOENT;
}

PBL_WEAK int settings_blob_db_delete(const uint8_t *key, int key_len) {
  return 0;
}

PBL_WEAK BlobDBDirtyItem *settings_blob_db_get_dirty_list(void) {
  return nullptr;
}

PBL_WEAK int settings_blob_db_mark_synced(const uint8_t *key, int key_len) {
  return 0;
}

PBL_WEAK int settings_blob_db_is_dirty(bool *is_dirty_out) {
  if (is_dirty_out) {
    *is_dirty_out = false;
  }
  return 0;
}

PBL_WEAK int settings_blob_db_flush(void) {
  return 0;
}

PBL_WEAK int settings_blob_db_mark_all_dirty(void) {
  return 0;
}

PBL_WEAK int settings_blob_db_insert_with_timestamp(const uint8_t *key, int key_len,
                                                    const uint8_t *val, int val_len,
                                                    time_t timestamp) {
  return 0;
}

PBL_WEAK bool settings_blob_db_phone_supports_sync(void) {
  return false;
}

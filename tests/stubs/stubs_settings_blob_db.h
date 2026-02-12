/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/api.h"

void settings_blob_db_init(void) {
}

status_t settings_blob_db_insert(const uint8_t *key, int key_len,
                                 const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int settings_blob_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

status_t settings_blob_db_read(const uint8_t *key, int key_len,
                               uint8_t *val_out, int val_len) {
  return E_DOES_NOT_EXIST;
}

status_t settings_blob_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

BlobDBDirtyItem *settings_blob_db_get_dirty_list(void) {
  return NULL;
}

status_t settings_blob_db_mark_synced(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t settings_blob_db_is_dirty(bool *is_dirty_out) {
  if (is_dirty_out) {
    *is_dirty_out = false;
  }
  return S_SUCCESS;
}

status_t settings_blob_db_flush(void) {
  return S_SUCCESS;
}

status_t settings_blob_db_mark_all_dirty(void) {
  return S_SUCCESS;
}

status_t settings_blob_db_insert_with_timestamp(const uint8_t *key, int key_len,
                                                const uint8_t *val, int val_len,
                                                time_t timestamp) {
  return S_SUCCESS;
}

bool settings_blob_db_phone_supports_sync(void) {
  return false;
}

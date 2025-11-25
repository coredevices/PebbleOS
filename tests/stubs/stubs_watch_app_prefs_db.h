/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/watch_app_prefs_db.h"

SerializedSendTextPrefs *watch_app_prefs_get_send_text(void);

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void watch_app_prefs_db_init(void) {
}

status_t watch_app_prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len) {
  return S_SUCCESS;
}

int watch_app_prefs_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

status_t watch_app_prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len) {
  return S_SUCCESS;
}

status_t watch_app_prefs_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t watch_app_prefs_db_flush(void) {
  return S_SUCCESS;
}

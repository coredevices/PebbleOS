/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "prefs_db.h"

#include "process_management/app_install_manager.h"
#include "shell/prefs_private.h"

// BlobDB APIs
////////////////////////////////////////////////////////////////////////////////

void prefs_db_init(void) {
}

status_t prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  bool success = prefs_private_write_backing(key, key_len, val, val_len);
  if (success) {
    return S_SUCCESS;
  } else {
    return E_INVALID_ARGUMENT;
  }
}

int prefs_db_get_len(const uint8_t *key, int key_len) {
  size_t len = prefs_private_get_backing_len(key, key_len);
  if (len == 0) {
    return E_INVALID_ARGUMENT;
  }
  return len;
}

status_t prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  bool success = prefs_private_read_backing(key, key_len, val_out, val_out_len);
  if (success) {
    return S_SUCCESS;
  } else {
    return E_INVALID_ARGUMENT;
  }
}

status_t prefs_db_delete(const uint8_t *key, int key_len) {
  return E_INVALID_OPERATION;
}

status_t prefs_db_flush(void) {
  return E_INVALID_OPERATION;
}

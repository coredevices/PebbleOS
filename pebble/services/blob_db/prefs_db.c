/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/services/blob_db/prefs_db.h>

#include <process_management/app_install_manager.h>
#include <shell/prefs_private.h>

// BlobDB APIs
////////////////////////////////////////////////////////////////////////////////

void prefs_db_init(void) {
}

int prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  bool success = prefs_private_write_backing(key, key_len, val, val_len);
  if (success) {
    return 0;
  } else {
    return -EINVAL;
  }
}

int prefs_db_get_len(const uint8_t *key, int key_len) {
  size_t len = prefs_private_get_backing_len(key, key_len);
  if (len == 0) {
    return -EINVAL;
  }
  return len;
}

int prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  bool success = prefs_private_read_backing(key, key_len, val_out, val_out_len);
  if (success) {
    return 0;
  } else {
    return -EINVAL;
  }
}

int prefs_db_delete(const uint8_t *key, int key_len) {
  return -ENOTSUP;
}

int prefs_db_flush(void) {
  return -ENOTSUP;
}

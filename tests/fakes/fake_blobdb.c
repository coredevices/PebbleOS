/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar_asserts.h"

#include <stddef.h>
#include <stdbool.h>

#include "test_db.h"
#include "services/normal/blob_db/api.h"

static BlobDBId s_blobdb_id = BlobDBIdTest;

void fake_blob_db_set_id(BlobDBId id) {
  s_blobdb_id = id;
}

void blob_db_init_dbs(void) {
  test_db_init();
}

void blob_db_get_dirty_dbs(uint8_t *ids, uint8_t *num_ids) {
  bool is_dirty = false;
  test_db_is_dirty(&is_dirty);
  if (is_dirty) {
    ids[0] = s_blobdb_id;
    *num_ids = 1;
  } else {
    *num_ids = 0;
  }
}

status_t blob_db_insert(BlobDBId db_id,
    const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_insert(key, key_len, val, val_len);
}

int blob_db_get_len(BlobDBId db_id,
    const uint8_t *key, int key_len) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_get_len(key, key_len);
}

status_t blob_db_read(BlobDBId db_id,
    const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_read(key, key_len, val_out, val_len);
}

status_t blob_db_delete(BlobDBId db_id,
    const uint8_t *key, int key_len) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_delete(key, key_len);
}

status_t blob_db_flush(BlobDBId db_id) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_flush();
}

BlobDBDirtyItem *blob_db_get_dirty_list(BlobDBId db_id) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_get_dirty_list();
}

status_t blob_db_mark_synced(BlobDBId db_id, uint8_t *key, int key_len) {
  cl_assert(db_id == s_blobdb_id);
  return test_db_mark_synced(key, key_len);
}

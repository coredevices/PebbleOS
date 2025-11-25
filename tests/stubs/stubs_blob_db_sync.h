/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/blob_db/sync.h"

status_t blob_db_sync_db(BlobDBId db_id) {
  return S_SUCCESS;
}

status_t blob_db_sync_record(BlobDBId db_id, const void *key, int key_len, time_t last_updated) {
  return S_SUCCESS;
}

BlobDBSyncSession *blob_db_sync_get_session_for_id(BlobDBId db_id) {
  return NULL;
}

BlobDBSyncSession *blob_db_sync_get_session_for_token(BlobDBToken token) {
  return NULL;
}

void blob_db_sync_next(BlobDBSyncSession *session) {
  return;
}

void blob_db_sync_cancel(BlobDBSyncSession *session) {
  return;
}

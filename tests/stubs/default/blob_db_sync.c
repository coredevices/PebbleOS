/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/sync.h>

PBL_WEAK status_t blob_db_sync_db(BlobDBId db_id) {
  return S_SUCCESS;
}

PBL_WEAK status_t blob_db_sync_record(BlobDBId db_id, const void *key, int key_len,
                                      time_t last_updated) {
  return S_SUCCESS;
}

PBL_WEAK BlobDBSyncSession *blob_db_sync_get_session_for_id(BlobDBId db_id) {
  return nullptr;
}

PBL_WEAK BlobDBSyncSession *blob_db_sync_get_session_for_token(BlobDBToken token) {
  return nullptr;
}

PBL_WEAK void blob_db_sync_next(BlobDBSyncSession *session) {
  return;
}

PBL_WEAK void blob_db_sync_cancel(BlobDBSyncSession *session) {
  return;
}

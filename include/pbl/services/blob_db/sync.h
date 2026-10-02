/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"
#include "endpoint.h"

#include "pbl/services/regular_timer.h"

/**
 * @defgroup services_blob_db_sync Sync
 * @ingroup services_blob_db
 * @brief Write-back of dirty records to the phone.
 *
 * Records are sent one at a time over the BlobDB sync endpoint; each acknowledgement marks the
 * record as synced and sends the next one. A write without answer is retried every 30 seconds
 * and the session is abandoned after 5 minutes.
 * @{
 */

/** @brief State of a sync session. */
typedef enum {
  /** No write in flight. */
  BlobDBSyncSessionStateIdle = 0,
  /** Waiting for the phone to acknowledge the current write. */
  BlobDBSyncSessionStateWaitingForAck = 1,
} BlobDBSyncSessionState;

/** @brief Scope of a sync session. */
typedef enum {
  /** Whole database, written back with WRITEBACK and ended with SYNC_DONE. */
  BlobDBSyncSessionTypeDB,
  /** Single record, written with WRITE. */
  BlobDBSyncSessionTypeRecord,
} BlobDBSyncSessionType;

/** @brief Sync session, writing the dirty records of a database back to the phone one at a time. */
typedef struct {
  /** List node. */
  ListNode node;
  /**
   * Unique id, never reused.
   *
   * Deferred timer callbacks carry this instead of a session pointer so they can detect a
   * session that has already been freed.
   */
  uint32_t session_id;
  /** Session state. */
  BlobDBSyncSessionState state;
  /** Database being synced. */
  BlobDBId db_id;
  /** Records left to sync; the head is the one in flight. */
  BlobDBDirtyItem *dirty_list;
  /** Retries the current write when the phone does not answer in time. */
  RegularTimerInfo timeout_timer;
  /** Cancels the session after repeated timeouts. */
  RegularTimerInfo abandon_timer;
  /** Token of the write in flight. */
  BlobDBToken current_token;
  /** Session scope. */
  BlobDBSyncSessionType session_type;
} BlobDBSyncSession;

/**
 * @brief Start writing back the dirty records of a database.
 *
 * Sends SYNC_DONE immediately when there is nothing to sync.
 *
 * @param db_id Database to sync.
 * @retval S_SUCCESS Sync started.
 * @retval S_NO_ACTION_REQUIRED No dirty records.
 * @retval E_BUSY A sync of this database is already in progress.
 * @retval E_INVALID_ARGUMENT Invalid database.
 */
status_t blob_db_sync_db(BlobDBId db_id);

/**
 * @brief Start writing a single record to the phone.
 *
 * If a whole-database sync is in progress, the record is picked up by it instead.
 *
 * @param db_id Database of the record.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param last_updated Modification time of the record.
 * @retval S_SUCCESS Sync started or deferred to the running session.
 * @retval E_INVALID_ARGUMENT Invalid database.
 */
status_t blob_db_sync_record(BlobDBId db_id, const void *key, int key_len, time_t last_updated);

/**
 * @brief Get the whole-database sync session of a database.
 *
 * Single-record sessions are not returned.
 *
 * @param db_id Database.
 * @return Session, NULL if no sync is in progress.
 */
BlobDBSyncSession *blob_db_sync_get_session_for_id(BlobDBId db_id);

/**
 * @brief Get the sync session waiting for a response with a given token.
 *
 * @param token Token of the write.
 * @return Session, NULL if none matches.
 */
BlobDBSyncSession *blob_db_sync_get_session_for_token(BlobDBToken token);

/**
 * @brief Mark the current record as synced and send the next one.
 *
 * When the list is exhausted, picks up records that became dirty meanwhile, or ends the
 * session and frees it.
 *
 * @param session Session; may be freed by the call.
 */
void blob_db_sync_next(BlobDBSyncSession *session);

/**
 * @brief Cancel a sync and free the session.
 *
 * Pending records are synced next time.
 *
 * @param session Session to cancel.
 */
void blob_db_sync_cancel(BlobDBSyncSession *session);

/** @} */

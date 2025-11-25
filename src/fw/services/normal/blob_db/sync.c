/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sync.h"
#include "endpoint.h"
#include "util.h"

#include "kernel/pbl_malloc.h"
#include "services/common/comm_session/session.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/list.h"

#include <stdlib.h>


#define SYNC_TIMEOUT_SECONDS 30

static BlobDBSyncSession *s_sync_sessions = NULL;

static bool prv_session_id_filter_callback(ListNode *node, void *data) {
  BlobDBId db_id = (BlobDBId)data;
  BlobDBSyncSession *session = (BlobDBSyncSession *)node;
  if (session->session_type == BlobDBSyncSessionTypeRecord) {
    return false;
  }
  return session->db_id == db_id;
}

static bool prv_session_token_filter_callback(ListNode *node, void *data) {
  uint16_t token = (uint16_t)(uintptr_t)data;
  BlobDBSyncSession *session = (BlobDBSyncSession *)node;
  return session->current_token == token;
}

static void prv_timeout_kernelbg_callback(void *data) {
  PBL_LOG(LOG_LEVEL_INFO, "Blob DB Sync timeout");
  BlobDBSyncSession *session = data;
  blob_db_sync_cancel(session);
}

static void prv_timeout_timer_callback(void *data) {
  system_task_add_callback(prv_timeout_kernelbg_callback, data);
}

static void prv_send_writeback(BlobDBSyncSession *session) {
  // we want to write-back the first item in the dirty list
  BlobDBDirtyItem *dirty_item = session->dirty_list;
  int item_size = blob_db_get_len(session->db_id, dirty_item->key, dirty_item->key_len);
  if (item_size == 0) {
    // item got removed during the sync. Go to the next one
    blob_db_sync_next(session);
    return;
  }

  if (!comm_session_get_system_session()) {
    PBL_LOG(LOG_LEVEL_INFO, "Cancelling sync: No route to phone");
    blob_db_sync_cancel(session);
    return;
  }

  // read item into a temporary buffer
  void *item_buf = kernel_malloc_check(item_size);
  status_t status = blob_db_read(session->db_id,
                                 dirty_item->key,
                                 dirty_item->key_len,
                                 item_buf, item_size);
  if (PASSED(status)) {
    regular_timer_add_multisecond_callback(&session->timeout_timer, SYNC_TIMEOUT_SECONDS);
  } else if (status == E_DOES_NOT_EXIST) {
    // item was removed
    blob_db_sync_next(session);
  } else {
    // something went terribly wrong
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read blob DB during sync. Error code: 0x%"PRIx32, status);
    blob_db_sync_cancel(session);
  }

  // only one writeback in flight at a time
  session->state = BlobDBSyncSessionStateWaitingForAck;


  if (session->session_type == BlobDBSyncSessionTypeDB) {
    session->current_token = blob_db_endpoint_send_writeback(session->db_id,
                                                             dirty_item->last_updated,
                                                             dirty_item->key,
                                                             dirty_item->key_len,
                                                             item_buf,
                                                             item_size);
  } else {
    session->current_token = blob_db_endpoint_send_write(session->db_id,
                                                         dirty_item->last_updated,
                                                         dirty_item->key,
                                                         dirty_item->key_len,
                                                         item_buf,
                                                         item_size);
  }

  kernel_free(item_buf);
}

BlobDBSyncSession* prv_create_sync_session(BlobDBId db_id, BlobDBDirtyItem *dirty_list,
                                           BlobDBSyncSessionType session_type) {
  BlobDBSyncSession *session = kernel_zalloc_check(sizeof(BlobDBSyncSession));
  session->state = BlobDBSyncSessionStateIdle;
  session->db_id = db_id;
  session->dirty_list = dirty_list;
  session->session_type = session_type;
  session->timeout_timer = (const RegularTimerInfo) {
    .cb = prv_timeout_timer_callback,
    .cb_data = session,
  };
  s_sync_sessions = (BlobDBSyncSession *)list_prepend((ListNode *)s_sync_sessions,
                                                      (ListNode *)session);

  return session;
}

//! Will not return sessions for individual records
BlobDBSyncSession *blob_db_sync_get_session_for_id(BlobDBId db_id) {
  return (BlobDBSyncSession *)list_find((ListNode *)s_sync_sessions,
                                        prv_session_id_filter_callback,
                                        (void *)(uintptr_t)db_id);
}

BlobDBSyncSession *blob_db_sync_get_session_for_token(uint16_t token) {
  return (BlobDBSyncSession *)list_find((ListNode *)s_sync_sessions,
                                        prv_session_token_filter_callback,
                                        (void *)(uintptr_t)token);
}

status_t blob_db_sync_db(BlobDBId db_id) {
  if (db_id >= NumBlobDBs) {
    return E_INVALID_ARGUMENT;
  }
  PBL_LOG(LOG_LEVEL_INFO, "Starting BlobDB db sync: %d", db_id);

  BlobDBDirtyItem *dirty_list = blob_db_get_dirty_list(db_id);
  if (!dirty_list) {
    blob_db_endpoint_send_sync_done(db_id);
    return S_NO_ACTION_REQUIRED;
  }

  BlobDBSyncSession *session = blob_db_sync_get_session_for_id(db_id);
  if (session) {
    // already have a session in progress!
    return E_BUSY;
  }

  session = prv_create_sync_session(db_id, dirty_list, BlobDBSyncSessionTypeDB);

  prv_send_writeback(session);

  return S_SUCCESS;
}

status_t blob_db_sync_record(BlobDBId db_id, const void *key, int key_len, time_t last_updated) {
  if (db_id >= NumBlobDBs) {
    return E_INVALID_ARGUMENT;
  }

  BlobDBSyncSession *session = blob_db_sync_get_session_for_id(db_id);
  if (session) {
    // This will get picked up by the current session when it is done with its dirty list
    return S_SUCCESS;
  }

  char buffer[key_len + 1];
  strncpy(buffer, (const char *)key, key_len);
  buffer[key_len] = '\0';
  PBL_LOG(LOG_LEVEL_INFO, "Starting BlobDB record sync: <%s>", buffer);

  BlobDBDirtyItem *dirty_list = kernel_zalloc_check(sizeof(BlobDBDirtyItem) + key_len);
  list_init((ListNode *)dirty_list);
  dirty_list->last_updated = last_updated;
  dirty_list->key_len = key_len;
  memcpy(dirty_list->key, key, key_len);

  session = prv_create_sync_session(db_id, dirty_list, BlobDBSyncSessionTypeRecord);

  prv_send_writeback(session);

  return S_SUCCESS;
}

void blob_db_sync_cancel(BlobDBSyncSession *session) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Cancelling session %d sync", session->db_id);
  if (regular_timer_is_scheduled(&session->timeout_timer)) {
    regular_timer_remove_callback(&session->timeout_timer);
  }
  blob_db_util_free_dirty_list(session->dirty_list);
  list_remove((ListNode *)session, (ListNode **)&s_sync_sessions, NULL);
  kernel_free(session);
}

void blob_db_sync_next(BlobDBSyncSession *session) {
  PBL_LOG(LOG_LEVEL_DEBUG, "blob_db_sync_next");
  BlobDBDirtyItem *dirty_item = session->dirty_list;
  blob_db_mark_synced(session->db_id, dirty_item->key, dirty_item->key_len);

  // we're done with this item, we pop it off the stack
  list_remove((ListNode *)dirty_item, (ListNode **)&session->dirty_list, NULL);
  kernel_free(dirty_item);

  if (session->dirty_list) {
    prv_send_writeback(session);
  } else {
    // Check if new records became dirty while syncing the current list
    // New records could have been added while we were syncing OR
    // the list could be incomplete because we ran out of memory
    session->dirty_list = blob_db_get_dirty_list(session->db_id);
    if (session->dirty_list) {
      prv_send_writeback(session);
    } else {
      PBL_LOG(LOG_LEVEL_INFO, "Finished syncing db %d, session type: %d", session->db_id,
                                                                          session->session_type);
      if (regular_timer_is_scheduled(&session->timeout_timer)) {
        regular_timer_remove_callback(&session->timeout_timer);
      }
      if (session->session_type == BlobDBSyncSessionTypeDB) {
        // Only send the sync done when syncing an entire db
        blob_db_endpoint_send_sync_done(session->db_id);
      }
      list_remove((ListNode *)session, (ListNode **)&s_sync_sessions, NULL);
      kernel_free(session);
    }
  }
}

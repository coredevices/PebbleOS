/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

// Fakes
/////////////
#include "fake_system_task.h"
#include "fake_regular_timer.h"
#include "fake_blobdb.h"

// Stubs
/////////////
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_session.h"

// FW Includes
///////////////
#include "services/normal/blob_db/api.h"
#include "services/normal/blob_db/util.h"
#include "services/normal/blob_db/sync.h"
#include "util/size.h"


// Writebacks counter
////////////////////////

static int s_num_writebacks;
static int s_num_until_timeout;

void blob_db_endpoint_send_sync_done(BlobDBId db_id) {
  return;
}

static void prv_handle_response_from_phone(void *data) {
  s_num_writebacks++;
  BlobDBSyncSession *session = data;
  blob_db_sync_next(session);
}

static void prv_generate_responses_from_phone(void) {
  while (fake_system_task_count_callbacks()) {
    fake_system_task_callbacks_invoke_pending();
  }
}



BlobDBToken blob_db_endpoint_send_writeback(BlobDBId db_id,
                                            time_t last_updated,
                                            const void *key,
                                            int key_len,
                                            const void *val,
                                            int val_len) {
  BlobDBSyncSession *session = blob_db_sync_get_session_for_id(db_id);
  cl_assert(session != NULL);
  if (s_num_until_timeout != 0 && s_num_writebacks >= s_num_until_timeout) {
    fake_regular_timer_trigger(&session->timeout_timer);
  } else {
    system_task_add_callback(prv_handle_response_from_phone, session);
  }

  return 12345;
}

BlobDBToken blob_db_endpoint_send_write(BlobDBId db_id,
                                        time_t last_updated,
                                        const void *key,
                                        int key_len,
                                        const void *val,
                                        int val_len) {
  return 0;
}

// Tests
////////////////////////

void test_blob_db_sync__initialize(void) {
  fake_blob_db_set_id(BlobDBIdTest);
  blob_db_init_dbs();
  s_num_until_timeout = 0;
  s_num_writebacks = 0;
}

void test_blob_db_sync__cleanup(void) {
  blob_db_flush(BlobDBIdTest);
  fake_system_task_callbacks_cleanup();
}

void test_blob_db_sync__no_dirty(void) {
  uint8_t ids[NumBlobDBs];
  uint8_t num_ids;
  blob_db_get_dirty_dbs(ids, &num_ids);
  cl_assert_equal_i(num_ids, 0);
  cl_assert(blob_db_get_dirty_list(BlobDBIdTest) == NULL);

  // insert one
  char *key = "key";
  char *value = "value";
  cl_assert_equal_i(S_SUCCESS, blob_db_insert(BlobDBIdTest,
                                              (uint8_t *)key,
                                              strlen(key),
                                              (uint8_t *)value,
                                              strlen(value)));
  blob_db_get_dirty_dbs(ids, &num_ids);
  cl_assert_equal_i(num_ids, 1);
  cl_assert(blob_db_get_dirty_list(BlobDBIdTest) != NULL);

  // mark it synced
  cl_assert_equal_i(S_SUCCESS, blob_db_mark_synced(BlobDBIdTest, (uint8_t *)key, strlen(key)));
  blob_db_get_dirty_dbs(ids, &num_ids);
  cl_assert_equal_i(num_ids, 0);
  cl_assert(blob_db_get_dirty_list(BlobDBIdTest) == NULL);
}

static bool prv_list_key_comparator(ListNode *cur_node, void *data) {
  BlobDBDirtyItem *dirty_item = (BlobDBDirtyItem *)cur_node;
  char *key = data;
  return (memcmp(dirty_item->key, key, dirty_item->key_len) == 0);
}

void test_blob_db_sync__dirty_list(void) {
  uint8_t ids[NumBlobDBs];
  uint8_t num_ids;
  blob_db_get_dirty_dbs(ids, &num_ids);
  cl_assert_equal_i(num_ids, 0);
  BlobDBDirtyItem *dirty_list = blob_db_get_dirty_list(BlobDBIdTest);
  cl_assert(dirty_list == NULL);
  blob_db_util_free_dirty_list(dirty_list);

  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // insert all keys
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  // check the dirty list
  blob_db_get_dirty_dbs(ids, &num_ids);
  cl_assert_equal_i(num_ids, 1);
  dirty_list = blob_db_get_dirty_list(BlobDBIdTest);
  cl_assert_equal_i(list_count(&dirty_list->node), ARRAY_LENGTH(keys));

  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    cl_assert(list_find(&dirty_list->node, prv_list_key_comparator, keys[i]) != NULL);
  }

  blob_db_util_free_dirty_list(dirty_list);

  // mark one as synced and re-check
  for (int synced_idx = 1; synced_idx < ARRAY_LENGTH(keys); ++synced_idx) {
    blob_db_mark_synced(BlobDBIdTest, (uint8_t *)keys[synced_idx - 1], key_len);
    dirty_list = blob_db_get_dirty_list(BlobDBIdTest);
    cl_assert_equal_i(list_count(&dirty_list->node), ARRAY_LENGTH(keys) - synced_idx);
    for (int i = synced_idx; i < ARRAY_LENGTH(keys); ++i) {
      cl_assert(list_find(&dirty_list->node, prv_list_key_comparator, keys[i]) != NULL);
    }
    blob_db_util_free_dirty_list(dirty_list);
  }
}

void test_blob_db_sync__sync_all(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // insert all keys
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);
  prv_generate_responses_from_phone();

  cl_assert_equal_i(s_num_writebacks, 5);
}

void test_blob_db_sync__sync_oom(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // insert all keys
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);

  // We have built the dirty list, add more entries.
  // This mimics us performing writes while the sync is ongoing or not having enough memory to
  // build the initial list
  char *extra_keys[] = { "keyA", "keyB" };
  char *extra_values[] = { "valA", "valB" };
  for (int i = 0; i < ARRAY_LENGTH(extra_keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)extra_keys[i], key_len,
                   (uint8_t *)extra_values[i], value_len);
  }

  prv_generate_responses_from_phone();

  cl_assert_equal_i(s_num_writebacks, 7);
}

void test_blob_db_sync__sync_some(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // insert all keys, mark some as synced
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
    // choose two that are not consecutive, that leaves 3 still to be synced
    if (i == 1 || i == 3) {
      blob_db_mark_synced(BlobDBIdTest, (uint8_t *)keys[i], key_len);
    }
  }

  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);
  prv_generate_responses_from_phone();

  cl_assert_equal_i(s_num_writebacks, 3);
}

void test_blob_db_sync__timeout_and_retry(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // insert all keys, mark some as synced
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  s_num_until_timeout = 3;
  s_num_writebacks = 0;
  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);
  prv_generate_responses_from_phone();
  cl_assert_equal_i(s_num_writebacks, s_num_until_timeout);
  s_num_until_timeout = 0;
  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);
  prv_generate_responses_from_phone();
  cl_assert_equal_i(s_num_writebacks, 5);
}

void test_blob_db_sync__sync_while_syncing(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(BlobDBIdTest, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  cl_assert(blob_db_sync_db(BlobDBIdTest) == S_SUCCESS);

  // We should throw an error if we get a sync while a db sync in in progress
  cl_assert(blob_db_sync_db(BlobDBIdTest) == E_BUSY);

  // Generate some responses so the sync session gets cleaned up
  prv_generate_responses_from_phone();
}

static void prv_fill_stop_return_session(BlobDBId id) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  fake_blob_db_set_id(id);
  blob_db_init_dbs();
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    blob_db_insert(id, (uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }
  s_num_writebacks = 0;
  cl_assert(blob_db_sync_db(id) == S_SUCCESS);
}

void test_blob_db_sync__find_session(void) {
  // create a few sync sessions
  prv_fill_stop_return_session(BlobDBIdTest);
  prv_fill_stop_return_session(BlobDBIdPins);
  prv_fill_stop_return_session(BlobDBIdReminders);

  // check we can conjure them by id
  BlobDBSyncSession *test_session = blob_db_sync_get_session_for_id(BlobDBIdTest);
  cl_assert(test_session);
  cl_assert_equal_i(test_session->db_id, BlobDBIdTest);
  BlobDBSyncSession *pins_session = blob_db_sync_get_session_for_id(BlobDBIdPins);
  cl_assert(pins_session);
  cl_assert_equal_i(pins_session->db_id, BlobDBIdPins);
  BlobDBSyncSession *reminders_session = blob_db_sync_get_session_for_id(BlobDBIdReminders);
  cl_assert(reminders_session);
  cl_assert_equal_i(reminders_session->db_id, BlobDBIdReminders);

  test_session->current_token = 1;
  pins_session->current_token = 2;
  reminders_session->current_token = 3;

  // check we can conjure them by token
  cl_assert(test_session == blob_db_sync_get_session_for_token(1));
  cl_assert(pins_session == blob_db_sync_get_session_for_token(2));
  cl_assert(reminders_session == blob_db_sync_get_session_for_token(3));

  // Cancel the sync sessions so they get cleaned up
  blob_db_sync_cancel(test_session);
  blob_db_sync_cancel(pins_session);
  blob_db_sync_cancel(reminders_session);

  // reset fake blob db so cleanup doesn't assert
  fake_blob_db_set_id(BlobDBIdTest);
  blob_db_init_dbs();
}

